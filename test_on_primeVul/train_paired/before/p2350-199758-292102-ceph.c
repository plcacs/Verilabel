int RGWPostObj_ObjStore_S3::get_tags()
{
  string tags_str;
  if (part_str(parts, "tagging", &tags_str)) {
    RGWObjTagsXMLParser parser;
    if (!parser.init()){
      ldout(s->cct, 0) << "Couldn't init RGWObjTags XML parser" << dendl;
      err_msg = "Server couldn't process the request";
      return -EINVAL; // TODO: This class of errors in rgw code should be a 5XX error
    }
    if (!parser.parse(tags_str.c_str(), tags_str.size(), 1)) {
      ldout(s->cct,0 ) << "Invalid Tagging XML" << dendl;
      err_msg = "Invalid Tagging XML";
      return -EINVAL;
    }

    RGWObjTagSet_S3 *obj_tags_s3;
    RGWObjTagging_S3 *tagging;

    tagging = static_cast<RGWObjTagging_S3 *>(parser.find_first("Tagging"));
    obj_tags_s3 = static_cast<RGWObjTagSet_S3 *>(tagging->find_first("TagSet"));
    if(!obj_tags_s3){
      return -ERR_MALFORMED_XML;
    }

    RGWObjTags obj_tags;
    int r = obj_tags_s3->rebuild(obj_tags);
    if (r < 0)
      return r;

    bufferlist tags_bl;
    obj_tags.encode(tags_bl);
    ldout(s->cct, 20) << "Read " << obj_tags.count() << "tags" << dendl;
    attrs[RGW_ATTR_TAGS] = tags_bl;
  }


  return 0;
}