load_image (const gchar  *filename,
            GError      **error)
{
  FILE     *fp;
  tga_info  info;
  guchar    header[18];
  guchar    footer[26];
  guchar    extension[495];
  long      offset;

  gint32 image_ID = -1;

  fp = g_fopen (filename, "rb");

  if (! fp)
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   _("Could not open '%s' for reading: %s"),
                   gimp_filename_to_utf8 (filename), g_strerror (errno));
      return -1;
    }

  gimp_progress_init_printf (_("Opening '%s'"),
                             gimp_filename_to_utf8 (filename));

  /* Is file big enough for a footer? */
  if (!fseek (fp, -26L, SEEK_END))
    {
      if (fread (footer, sizeof (footer), 1, fp) != 1)
        {
          g_message (_("Cannot read footer from '%s'"),
                     gimp_filename_to_utf8 (filename));
          return -1;
        }
      else if (memcmp (footer + 8, magic, sizeof (magic)) == 0)
        {
          /* Check the signature. */

          offset = (footer[0]          +
                    footer[1] * 256L   +
                    footer[2] * 65536L +
                    footer[3] * 16777216L);

          if (offset != 0)
            {
              if (fseek (fp, offset, SEEK_SET) ||
                  fread (extension, sizeof (extension), 1, fp) != 1)
                {
                  g_message (_("Cannot read extension from '%s'"),
                             gimp_filename_to_utf8 (filename));
                  return -1;
                }
              /* Eventually actually handle version 2 TGA here */
            }
        }
    }

  if (fseek (fp, 0, SEEK_SET) ||
      fread (header, sizeof (header), 1, fp) != 1)
    {
      g_message (_("Cannot read header from '%s'"),
                 gimp_filename_to_utf8 (filename));
      return -1;
    }

  switch (header[2])
    {
    case 1:
      info.imageType        = TGA_TYPE_MAPPED;
      info.imageCompression = TGA_COMP_NONE;
      break;
    case 2:
      info.imageType        = TGA_TYPE_COLOR;
      info.imageCompression = TGA_COMP_NONE;
      break;
    case 3:
      info.imageType        = TGA_TYPE_GRAY;
      info.imageCompression = TGA_COMP_NONE;
      break;

    case 9:
      info.imageType        = TGA_TYPE_MAPPED;
      info.imageCompression = TGA_COMP_RLE;
      break;
    case 10:
      info.imageType        = TGA_TYPE_COLOR;
      info.imageCompression = TGA_COMP_RLE;
      break;
    case 11:
      info.imageType        = TGA_TYPE_GRAY;
      info.imageCompression = TGA_COMP_RLE;
      break;

    default:
      info.imageType = 0;
    }

  info.idLength     = header[0];
  info.colorMapType = header[1];

  info.colorMapIndex  = header[3] + header[4] * 256;
  info.colorMapLength = header[5] + header[6] * 256;
  info.colorMapSize   = header[7];

  info.xOrigin = header[8]  + header[9] * 256;
  info.yOrigin = header[10] + header[11] * 256;
  info.width   = header[12] + header[13] * 256;
  info.height  = header[14] + header[15] * 256;

  info.bpp       = header[16];
  info.bytes     = (info.bpp + 7) / 8;
  info.alphaBits = header[17] & 0x0f; /* Just the low 4 bits */
  info.flipHoriz = (header[17] & 0x10) ? 1 : 0;
  info.flipVert  = (header[17] & 0x20) ? 0 : 1;

  /* hack to handle some existing files with incorrect headers, see bug #306675 */
  if (info.alphaBits == info.bpp)
    info.alphaBits = 0;

  /* hack to handle yet another flavor of incorrect headers, see bug #540969 */
  if (info.alphaBits == 0)
    {
      if (info.imageType == TGA_TYPE_COLOR && info.bpp == 32)
        info.alphaBits = 8;

      if (info.imageType == TGA_TYPE_GRAY && info.bpp == 16)
        info.alphaBits = 8;
    }

  switch (info.imageType)
    {
      case TGA_TYPE_MAPPED:
        if (info.bpp != 8)
          {
            g_message ("Unhandled sub-format in '%s' (type = %u, bpp = %u)",
                       gimp_filename_to_utf8 (filename),
                       info.imageType, info.bpp);
            return -1;
          }
        break;
      case TGA_TYPE_COLOR:
        if ((info.bpp != 15 && info.bpp != 16 &&
             info.bpp != 24 && info.bpp != 32)      ||
            ((info.bpp == 15 || info.bpp == 24) &&
             info.alphaBits != 0)                   ||
            (info.bpp == 16 && info.alphaBits != 1) ||
            (info.bpp == 32 && info.alphaBits != 8))
          {
            g_message ("Unhandled sub-format in '%s' (type = %u, bpp = %u, alpha = %u)",
                       gimp_filename_to_utf8 (filename),
                       info.imageType, info.bpp, info.alphaBits);
            return -1;
          }
        break;
      case TGA_TYPE_GRAY:
        if (info.bpp != 8 &&
            (info.alphaBits != 8 || (info.bpp != 16 && info.bpp != 15)))
          {
            g_message ("Unhandled sub-format in '%s' (type = %u, bpp = %u)",
                       gimp_filename_to_utf8 (filename),
                       info.imageType, info.bpp);
            return -1;
          }
        break;

      default:
        g_message ("Unknown image type %u for '%s'",
                   info.imageType, gimp_filename_to_utf8 (filename));
        return -1;
    }

  /* Plausible but unhandled formats */
  if (info.bytes * 8 != info.bpp && info.bpp != 15)
    {
      g_message ("Unhandled sub-format in '%s' (type = %u, bpp = %u)",
                 gimp_filename_to_utf8 (filename),
                 info.imageType, info.bpp);
      return -1;
    }

  /* Check that we have a color map only when we need it. */
  if (info.imageType == TGA_TYPE_MAPPED && info.colorMapType != 1)
    {
      g_message ("Indexed image has invalid color map type %u",
                 info.colorMapType);
      return -1;
    }
  else if (info.imageType != TGA_TYPE_MAPPED && info.colorMapType != 0)
    {
      g_message ("Non-indexed image has invalid color map type %u",
                 info.colorMapType);
      return -1;
    }

  /* Skip the image ID field. */
  if (info.idLength && fseek (fp, info.idLength, SEEK_CUR))
    {
      g_message ("File '%s' is truncated or corrupted",
                 gimp_filename_to_utf8 (filename));
      return -1;
    }

  image_ID = ReadImage (fp, &info, filename);

  fclose (fp);

  return image_ID;
}