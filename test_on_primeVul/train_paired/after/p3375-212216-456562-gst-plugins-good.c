qtdemux_tag_add_str_full (GstQTDemux * qtdemux, GstTagList * taglist,
    const char *tag, const char *dummy, GNode * node)
{
  const gchar *env_vars[] = { "GST_QT_TAG_ENCODING", "GST_TAG_ENCODING", NULL };
  GNode *data;
  char *s;
  int len;
  guint32 type;
  int offset;
  gboolean ret = TRUE;
  const gchar *charset = NULL;

  data = qtdemux_tree_get_child_by_type (node, FOURCC_data);
  if (data) {
    len = QT_UINT32 (data->data);
    type = QT_UINT32 ((guint8 *) data->data + 8);
    if (type == 0x00000001 && len > 16) {
      s = gst_tag_freeform_string_to_utf8 ((char *) data->data + 16, len - 16,
          env_vars);
      if (s) {
        GST_DEBUG_OBJECT (qtdemux, "adding tag %s", GST_STR_NULL (s));
        gst_tag_list_add (taglist, GST_TAG_MERGE_REPLACE, tag, s, NULL);
        g_free (s);
      } else {
        GST_DEBUG_OBJECT (qtdemux, "failed to convert %s tag to UTF-8", tag);
      }
    }
  } else {
    len = QT_UINT32 (node->data);
    type = QT_UINT32 ((guint8 *) node->data + 4);
    if ((type >> 24) == 0xa9 && len > 8 + 4) {
      gint str_len;
      gint lang_code;

      /* Type starts with the (C) symbol, so the next data is a list
       * of (string size(16), language code(16), string) */

      str_len = QT_UINT16 ((guint8 *) node->data + 8);
      lang_code = QT_UINT16 ((guint8 *) node->data + 10);

      /* the string + fourcc + size + 2 16bit fields,
       * means that there are more tags in this atom */
      if (len > str_len + 8 + 4) {
        /* TODO how to represent the same tag in different languages? */
        GST_WARNING_OBJECT (qtdemux, "Ignoring metadata entry with multiple "
            "text alternatives, reading only first one");
      }

      offset = 12;
      len = MIN (len, str_len + 8 + 4); /* remove trailing strings that we don't use */
      GST_DEBUG_OBJECT (qtdemux, "found international text tag");

      if (lang_code < 0x800) {  /* MAC encoded string */
        charset = "mac";
      }
    } else if (len > 14 && qtdemux_is_string_tag_3gp (qtdemux,
            QT_FOURCC ((guint8 *) node->data + 4))) {
      guint32 type = QT_UINT32 ((guint8 *) node->data + 8);

      /* we go for 3GP style encoding if major brands claims so,
       * or if no hope for data be ok UTF-8, and compatible 3GP brand present */
      if (qtdemux_is_brand_3gp (qtdemux, TRUE) ||
          (qtdemux_is_brand_3gp (qtdemux, FALSE) &&
              ((type & 0x00FFFFFF) == 0x0) && (type >> 24 <= 0xF))) {
        offset = 14;
        /* 16-bit Language code is ignored here as well */
        GST_DEBUG_OBJECT (qtdemux, "found 3gpp text tag");
      } else {
        goto normal;
      }
    } else {
    normal:
      offset = 8;
      GST_DEBUG_OBJECT (qtdemux, "found normal text tag");
      ret = FALSE;              /* may have to fallback */
    }
    if (charset) {
      GError *err = NULL;

      s = g_convert ((gchar *) node->data + offset, len - offset, "utf8",
          charset, NULL, NULL, &err);
      if (err) {
        GST_DEBUG_OBJECT (qtdemux, "Failed to convert string from charset %s:"
            " %s(%d): %s", charset, g_quark_to_string (err->domain), err->code,
            err->message);
        g_error_free (err);
      }
    } else {
      s = gst_tag_freeform_string_to_utf8 ((char *) node->data + offset,
          len - offset, env_vars);
    }
    if (s) {
      GST_DEBUG_OBJECT (qtdemux, "adding tag %s", GST_STR_NULL (s));
      gst_tag_list_add (taglist, GST_TAG_MERGE_REPLACE, tag, s, NULL);
      g_free (s);
      ret = TRUE;
    } else {
      GST_DEBUG_OBJECT (qtdemux, "failed to convert %s tag to UTF-8", tag);
    }
  }
  return ret;
}