hb_ot_layout_build_glyph_classes (hb_face_t      *face,
				  uint16_t        num_total_glyphs,
				  hb_codepoint_t *glyphs,
				  unsigned char  *klasses,
				  uint16_t        count)
{
  if (HB_OBJECT_IS_INERT (face))
    return;

  hb_ot_layout_t *layout = &face->ot_layout;

  if (HB_UNLIKELY (!count || !glyphs || !klasses))
    return;

  if (layout->new_gdef.len == 0) {
    layout->new_gdef.klasses = (unsigned char *) calloc (num_total_glyphs, sizeof (unsigned char));
    layout->new_gdef.len = count;
  }

  for (unsigned int i = 0; i < count; i++)
    _hb_ot_layout_set_glyph_class (face, glyphs[i], (hb_ot_layout_glyph_class_t) klasses[i]);
}