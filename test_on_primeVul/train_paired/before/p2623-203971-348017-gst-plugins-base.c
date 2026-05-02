windows_icon_typefind (GstTypeFind * find, gpointer user_data)
{
  const guint8 *data;
  gint64 datalen;
  guint16 type, nimages;
  gint32 size, offset;

  datalen = gst_type_find_get_length (find);
  if ((data = gst_type_find_peek (find, 0, 6)) == NULL)
    return;

  /* header - simple and not enough to rely on it alone */
  if (GST_READ_UINT16_LE (data) != 0)
    return;
  type = GST_READ_UINT16_LE (data + 2);
  if (type != 1 && type != 2)
    return;
  nimages = GST_READ_UINT16_LE (data + 4);
  if (nimages == 0)             /* we can assume we can't have an empty image file ? */
    return;

  /* first image */
  if (data[6 + 3] != 0)
    return;
  if (type == 1) {
    guint16 planes = GST_READ_UINT16_LE (data + 6 + 4);
    if (planes > 1)
      return;
  }
  size = GST_READ_UINT32_LE (data + 6 + 8);
  offset = GST_READ_UINT32_LE (data + 6 + 12);
  if (offset < 0 || size <= 0 || size >= datalen || offset >= datalen
      || size + offset > datalen)
    return;

  gst_type_find_suggest_simple (find, GST_TYPE_FIND_NEARLY_CERTAIN,
      "image/x-icon", NULL);
}