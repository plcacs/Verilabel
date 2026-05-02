read_creator_block (FILE     *f,
                    gint      image_ID,
                    guint     total_len,
                    PSPimage *ia)
{
  long          data_start;
  guchar        buf[4];
  guint16       keyword;
  guint32       length;
  gchar        *string;
  gchar        *title = NULL, *artist = NULL, *copyright = NULL, *description = NULL;
  guint32       dword;
  guint32       __attribute__((unused))cdate = 0;
  guint32       __attribute__((unused))mdate = 0;
  guint32       __attribute__((unused))appid;
  guint32       __attribute__((unused))appver;
  GString      *comment;
  GimpParasite *comment_parasite;

  data_start = ftell (f);
  comment = g_string_new (NULL);

  while (ftell (f) < data_start + total_len)
    {
      if (fread (buf, 4, 1, f) < 1
          || fread (&keyword, 2, 1, f) < 1
          || fread (&length, 4, 1, f) < 1)
        {
          g_message ("Error reading creator keyword chunk");
          return -1;
        }
      if (memcmp (buf, "~FL\0", 4) != 0)
        {
          g_message ("Invalid keyword chunk header");
          return -1;
        }
      keyword = GUINT16_FROM_LE (keyword);
      length = GUINT32_FROM_LE (length);
      switch (keyword)
        {
        case PSP_CRTR_FLD_TITLE:
        case PSP_CRTR_FLD_ARTIST:
        case PSP_CRTR_FLD_CPYRGHT:
        case PSP_CRTR_FLD_DESC:
          string = g_malloc (length + 1);
          if (fread (string, length, 1, f) < 1)
            {
              g_message ("Error reading creator keyword data");
              g_free (string);
              return -1;
            }
          switch (keyword)
            {
            case PSP_CRTR_FLD_TITLE:
              g_free (title); title = string; break;
            case PSP_CRTR_FLD_ARTIST:
              g_free (artist); artist = string; break;
            case PSP_CRTR_FLD_CPYRGHT:
              g_free (copyright); copyright = string; break;
            case PSP_CRTR_FLD_DESC:
              g_free (description); description = string; break;
            default:
              g_free (string);
            }
          break;
        case PSP_CRTR_FLD_CRT_DATE:
        case PSP_CRTR_FLD_MOD_DATE:
        case PSP_CRTR_FLD_APP_ID:
        case PSP_CRTR_FLD_APP_VER:
          if (fread (&dword, 4, 1, f) < 1)
            {
              g_message ("Error reading creator keyword data");
              return -1;
            }
          switch (keyword)
            {
            case PSP_CRTR_FLD_CRT_DATE:
              cdate = dword; break;
            case PSP_CRTR_FLD_MOD_DATE:
              mdate = dword; break;
            case PSP_CRTR_FLD_APP_ID:
              appid = dword; break;
            case PSP_CRTR_FLD_APP_VER:
              appver = dword; break;
            }
          break;
        default:
          if (try_fseek (f, length, SEEK_CUR) < 0)
            {
              return -1;
            }
          break;
        }
    }

  if (title)
    {
      g_string_append (comment, title);
      g_free (title);
      g_string_append (comment, "\n");
    }
  if (artist)
    {
      g_string_append (comment, artist);
      g_free (artist);
      g_string_append (comment, "\n");
    }
  if (copyright)
    {
      g_string_append (comment, "Copyright ");
      g_string_append (comment, copyright);
      g_free (copyright);
      g_string_append (comment, "\n");
    }
  if (description)
    {
      g_string_append (comment, description);
      g_free (description);
      g_string_append (comment, "\n");
    }
  if (comment->len > 0)
    {
      comment_parasite = gimp_parasite_new ("gimp-comment",
                                            GIMP_PARASITE_PERSISTENT,
                                            strlen (comment->str) + 1,
                                            comment->str);
      gimp_image_attach_parasite (image_ID, comment_parasite);
      gimp_parasite_free (comment_parasite);
    }

  g_string_free (comment, FALSE);

  return 0;
}