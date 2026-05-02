bool RGWSwiftWebsiteHandler::is_web_dir() const
{
  std::string subdir_name = url_decode(s->object.name);

  /* Remove character from the subdir name if it is "/". */
  if (subdir_name.empty()) {
    return false;
  } else if (subdir_name.back() == '/') {
    subdir_name.pop_back();
  }

  rgw_obj obj(s->bucket, std::move(subdir_name));

  /* First, get attrset of the object we'll try to retrieve. */
  RGWObjectCtx& obj_ctx = *static_cast<RGWObjectCtx *>(s->obj_ctx);
  obj_ctx.set_atomic(obj);
  obj_ctx.set_prefetch_data(obj);

  RGWObjState* state = nullptr;
  if (store->get_obj_state(&obj_ctx, s->bucket_info, obj, &state, false) < 0) {
    return false;
  }

  /* A nonexistent object cannot be a considered as a marker representing
   * the emulation of catalog in FS hierarchy. */
  if (! state->exists) {
    return false;
  }

  /* Decode the content type. */
  std::string content_type;
  get_contype_from_attrs(state->attrset, content_type);

  const auto& ws_conf = s->bucket_info.website_conf;
  const std::string subdir_marker = ws_conf.subdir_marker.empty()
                                      ? "application/directory"
                                      : ws_conf.subdir_marker;
  return subdir_marker == content_type && state->size <= 1;
}