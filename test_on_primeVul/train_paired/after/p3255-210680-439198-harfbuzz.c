  inline void init (hb_face_t *face,
		    hb_tag_t _hea_tag, hb_tag_t _mtx_tag,
		    unsigned int default_advance_)
  {
    this->default_advance = default_advance_;
    this->num_metrics = face->get_num_glyphs ();

    hb_blob_t *_hea_blob = OT::Sanitizer<OT::_hea>::sanitize (face->reference_table (_hea_tag));
    const OT::_hea *_hea = OT::Sanitizer<OT::_hea>::lock_instance (_hea_blob);
    this->num_advances = _hea->numberOfLongMetrics;
    hb_blob_destroy (_hea_blob);

    this->blob = OT::Sanitizer<OT::_mtx>::sanitize (face->reference_table (_mtx_tag));
    if (unlikely (!this->num_advances ||
		  2 * (this->num_advances + this->num_metrics) > hb_blob_get_length (this->blob)))
    {
      this->num_metrics = this->num_advances = 0;
      hb_blob_destroy (this->blob);
      this->blob = hb_blob_get_empty ();
    }
    this->table = OT::Sanitizer<OT::_mtx>::lock_instance (this->blob);
  }