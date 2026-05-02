int RGWGetObj_ObjStore_S3::send_response_data(bufferlist& bl, off_t bl_ofs,
					      off_t bl_len)
{
  const char *content_type = NULL;
  string content_type_str;
  map<string, string> response_attrs;
  map<string, string>::iterator riter;
  bufferlist metadata_bl;

  if (sent_header)
    goto send_data;

  if (custom_http_ret) {
    set_req_state_err(s, 0);
    dump_errno(s, custom_http_ret);
  } else {
    set_req_state_err(s, (partial_content && !op_ret) ? STATUS_PARTIAL_CONTENT
                  : op_ret);
    dump_errno(s);
  }

  if (op_ret)
    goto done;

  if (range_str)
    dump_range(s, start, end, s->obj_size);

  if (s->system_request &&
      s->info.args.exists(RGW_SYS_PARAM_PREFIX "prepend-metadata")) {

    dump_header(s, "Rgwx-Object-Size", (long long)total_len);

    if (rgwx_stat) {
      /*
       * in this case, we're not returning the object's content, only the prepended
       * extra metadata
       */
      total_len = 0;
    }

    /* JSON encode object metadata */
    JSONFormatter jf;
    jf.open_object_section("obj_metadata");
    encode_json("attrs", attrs, &jf);
    utime_t ut(lastmod);
    encode_json("mtime", ut, &jf);
    jf.close_section();
    stringstream ss;
    jf.flush(ss);
    metadata_bl.append(ss.str());
    dump_header(s, "Rgwx-Embedded-Metadata-Len", metadata_bl.length());
    total_len += metadata_bl.length();
  }

  if (s->system_request && !real_clock::is_zero(lastmod)) {
    /* we end up dumping mtime in two different methods, a bit redundant */
    dump_epoch_header(s, "Rgwx-Mtime", lastmod);
    uint64_t pg_ver = 0;
    int r = decode_attr_bl_single_value(attrs, RGW_ATTR_PG_VER, &pg_ver, (uint64_t)0);
    if (r < 0) {
      ldout(s->cct, 0) << "ERROR: failed to decode pg ver attr, ignoring" << dendl;
    }
    dump_header(s, "Rgwx-Obj-PG-Ver", pg_ver);

    uint32_t source_zone_short_id = 0;
    r = decode_attr_bl_single_value(attrs, RGW_ATTR_SOURCE_ZONE, &source_zone_short_id, (uint32_t)0);
    if (r < 0) {
      ldout(s->cct, 0) << "ERROR: failed to decode pg ver attr, ignoring" << dendl;
    }
    if (source_zone_short_id != 0) {
      dump_header(s, "Rgwx-Source-Zone-Short-Id", source_zone_short_id);
    }
  }

  for (auto &it : crypt_http_responses)
    dump_header(s, it.first, it.second);

  dump_content_length(s, total_len);
  dump_last_modified(s, lastmod);
  dump_header_if_nonempty(s, "x-amz-version-id", version_id);
  if (attrs.find(RGW_ATTR_APPEND_PART_NUM) != attrs.end()) {
    dump_header(s, "x-rgw-object-type", "Appendable");
    dump_header(s, "x-rgw-next-append-position", s->obj_size);
  } else {
    dump_header(s, "x-rgw-object-type", "Normal");
  }

  if (! op_ret) {
    if (! lo_etag.empty()) {
      /* Handle etag of Swift API's large objects (DLO/SLO). It's entirerly
       * legit to perform GET on them through S3 API. In such situation,
       * a client should receive the composited content with corresponding
       * etag value. */
      dump_etag(s, lo_etag);
    } else {
      auto iter = attrs.find(RGW_ATTR_ETAG);
      if (iter != attrs.end()) {
        dump_etag(s, iter->second.to_str());
      }
    }

    for (struct response_attr_param *p = resp_attr_params; p->param; p++) {
      bool exists;
      string val = s->info.args.get(p->param, &exists);
      if (exists) {
	/* reject unauthenticated response header manipulation, see
	 * https://docs.aws.amazon.com/AmazonS3/latest/API/API_GetObject.html */
	if (s->auth.identity->is_anonymous()) {
	  return -ERR_INVALID_REQUEST;
	}
	if (strcmp(p->param, "response-content-type") != 0) {
	  response_attrs[p->http_attr] = val;
	} else {
	  content_type_str = val;
	  content_type = content_type_str.c_str();
	}
      }
    }

    for (auto iter = attrs.begin(); iter != attrs.end(); ++iter) {
      const char *name = iter->first.c_str();
      map<string, string>::iterator aiter = rgw_to_http_attrs.find(name);
      if (aiter != rgw_to_http_attrs.end()) {
        if (response_attrs.count(aiter->second) == 0) {
          /* Was not already overridden by a response param. */

          size_t len = iter->second.length();
          string s(iter->second.c_str(), len);
          while (len && !s[len - 1]) {
            --len;
            s.resize(len);
          }
          response_attrs[aiter->second] = s;
        }
      } else if (iter->first.compare(RGW_ATTR_CONTENT_TYPE) == 0) {
        /* Special handling for content_type. */
        if (!content_type) {
          content_type_str = rgw_bl_str(iter->second);
          content_type = content_type_str.c_str();
        }
      } else if (strcmp(name, RGW_ATTR_SLO_UINDICATOR) == 0) {
        // this attr has an extra length prefix from encode() in prior versions
        dump_header(s, "X-Object-Meta-Static-Large-Object", "True");
      } else if (strncmp(name, RGW_ATTR_META_PREFIX,
			 sizeof(RGW_ATTR_META_PREFIX)-1) == 0) {
        /* User custom metadata. */
        name += sizeof(RGW_ATTR_PREFIX) - 1;
        dump_header(s, name, iter->second);
      } else if (iter->first.compare(RGW_ATTR_TAGS) == 0) {
        RGWObjTags obj_tags;
        try{
          auto it = iter->second.cbegin();
          obj_tags.decode(it);
        } catch (buffer::error &err) {
          ldout(s->cct,0) << "Error caught buffer::error couldn't decode TagSet " << dendl;
        }
        dump_header(s, RGW_AMZ_TAG_COUNT, obj_tags.count());
      } else if (iter->first.compare(RGW_ATTR_OBJECT_RETENTION) == 0 && get_retention){
        RGWObjectRetention retention;
        try {
          decode(retention, iter->second);
          dump_header(s, "x-amz-object-lock-mode", retention.get_mode());
          dump_time_header(s, "x-amz-object-lock-retain-until-date", retention.get_retain_until_date());
        } catch (buffer::error& err) {
          ldpp_dout(this, 0) << "ERROR: failed to decode RGWObjectRetention" << dendl;
        }
      } else if (iter->first.compare(RGW_ATTR_OBJECT_LEGAL_HOLD) == 0 && get_legal_hold) {
        RGWObjectLegalHold legal_hold;
        try {
          decode(legal_hold, iter->second);
          dump_header(s, "x-amz-object-lock-legal-hold",legal_hold.get_status());
        } catch (buffer::error& err) {
          ldpp_dout(this, 0) << "ERROR: failed to decode RGWObjectLegalHold" << dendl;
        }
      }
    }
  }

done:
  for (riter = response_attrs.begin(); riter != response_attrs.end();
       ++riter) {
    dump_header(s, riter->first, riter->second);
  }

  if (op_ret == -ERR_NOT_MODIFIED) {
      end_header(s, this);
  } else {
      if (!content_type)
          content_type = "binary/octet-stream";

      end_header(s, this, content_type);
  }

  if (metadata_bl.length()) {
    dump_body(s, metadata_bl);
  }
  sent_header = true;

send_data:
  if (get_data && !op_ret) {
    int r = dump_body(s, bl.c_str() + bl_ofs, bl_len);
    if (r < 0)
      return r;
  }

  return 0;
}