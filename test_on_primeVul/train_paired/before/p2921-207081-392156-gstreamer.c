gst_date_time_new_from_iso8601_string (const gchar * string)
{
  gint year = -1, month = -1, day = -1, hour = -1, minute = -1;
  gint gmt_offset_hour = -99, gmt_offset_min = -99;
  gdouble second = -1.0;
  gfloat tzoffset = 0.0;
  guint64 usecs;
  gint len, ret;

  g_return_val_if_fail (string != NULL, NULL);

  GST_DEBUG ("Parsing '%s' into a datetime", string);

  len = strlen (string);

  /* The input string is expected to start either with a year (4 digits) or
   * with an hour (2 digits). Hour must be followed by minute. In any case,
   * the string must be at least 4 characters long and start with 2 digits */
  if (len < 4 || !g_ascii_isdigit (string[0]) || !g_ascii_isdigit (string[1]))
    return NULL;

  if (g_ascii_isdigit (string[2]) && g_ascii_isdigit (string[3])) {
    ret = sscanf (string, "%04d-%02d-%02d", &year, &month, &day);

    if (ret == 0)
      return NULL;

    if (ret == 3 && day <= 0) {
      ret = 2;
      day = -1;
    }

    if (ret >= 2 && month <= 0) {
      ret = 1;
      month = day = -1;
    }

    if (ret >= 1 && (year <= 0 || year > 9999 || month > 12 || day > 31))
      return NULL;

    else if (ret >= 1 && len < 16)
      /* YMD is 10 chars. XMD + HM will be 16 chars. if it is less,
       * it make no sense to continue. We will stay with YMD. */
      goto ymd;

    string += 10;
    /* Exit if there is no expeceted value on this stage */
    if (!(*string == 'T' || *string == '-' || *string == ' '))
      goto ymd;

    string += 1;
  }
  /* if hour or minute fails, then we will use only ymd. */
  hour = g_ascii_strtoull (string, (gchar **) & string, 10);
  if (hour > 24 || *string != ':')
    goto ymd;

  /* minute */
  minute = g_ascii_strtoull (string + 1, (gchar **) & string, 10);
  if (minute > 59)
    goto ymd;

  /* second */
  if (*string == ':') {
    second = g_ascii_strtoull (string + 1, (gchar **) & string, 10);
    /* if we fail here, we still can reuse hour and minute. We
     * will still attempt to parse any timezone information */
    if (second > 59) {
      second = -1.0;
    } else {
      /* microseconds */
      if (*string == '.' || *string == ',') {
        const gchar *usec_start = string + 1;
        guint digits;

        usecs = g_ascii_strtoull (string + 1, (gchar **) & string, 10);
        if (usecs != G_MAXUINT64 && string > usec_start) {
          digits = (guint) (string - usec_start);
          second += (gdouble) usecs / pow (10.0, digits);
        }
      }
    }
  }

  if (*string == 'Z')
    goto ymd_hms;
  else {
    /* reuse some code from gst-plugins-base/gst-libs/gst/tag/gstxmptag.c */
    gint gmt_offset = -1;
    gchar *plus_pos = NULL;
    gchar *neg_pos = NULL;
    gchar *pos = NULL;

    GST_LOG ("Checking for timezone information");

    /* check if there is timezone info */
    plus_pos = strrchr (string, '+');
    neg_pos = strrchr (string, '-');
    if (plus_pos)
      pos = plus_pos + 1;
    else if (neg_pos)
      pos = neg_pos + 1;

    if (pos) {
      gint ret_tz;
      if (pos[2] == ':')
        ret_tz = sscanf (pos, "%d:%d", &gmt_offset_hour, &gmt_offset_min);
      else
        ret_tz = sscanf (pos, "%02d%02d", &gmt_offset_hour, &gmt_offset_min);

      GST_DEBUG ("Parsing timezone: %s", pos);

      if (ret_tz == 2) {
        if (neg_pos != NULL && neg_pos + 1 == pos) {
          gmt_offset_hour *= -1;
          gmt_offset_min *= -1;
        }
        gmt_offset = gmt_offset_hour * 60 + gmt_offset_min;

        tzoffset = gmt_offset / 60.0;

        GST_LOG ("Timezone offset: %f (%d minutes)", tzoffset, gmt_offset);
      } else
        GST_WARNING ("Failed to parse timezone information");
    }
  }

ymd_hms:
  if (year == -1 || month == -1 || day == -1) {
    GDateTime *now_utc, *now_in_given_tz;

    /* No date was supplied: make it today */
    now_utc = g_date_time_new_now_utc ();
    if (tzoffset != 0.0) {
      /* If a timezone offset was supplied, get the date of that timezone */
      g_assert (gmt_offset_min != -99);
      g_assert (gmt_offset_hour != -99);
      now_in_given_tz =
          g_date_time_add_minutes (now_utc,
          (60 * gmt_offset_hour) + gmt_offset_min);
      g_date_time_unref (now_utc);
    } else {
      now_in_given_tz = now_utc;
    }
    g_date_time_get_ymd (now_in_given_tz, &year, &month, &day);
    g_date_time_unref (now_in_given_tz);
  }
  return gst_date_time_new (tzoffset, year, month, day, hour, minute, second);
ymd:
  if (year == -1) {
    /* No date was supplied and time failed to parse */
    return NULL;
  }
  return gst_date_time_new_ymd (year, month, day);
}