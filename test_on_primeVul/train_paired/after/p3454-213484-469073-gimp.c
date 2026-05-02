read_channel_data (FILE       *f,
                   PSPimage   *ia,
                   guchar    **pixels,
                   guint       bytespp,
                   guint       offset,
                   GimpDrawable  *drawable,
                   guint32     compressed_len)
{
  gint i, y, width = drawable->width, height = drawable->height;
  gint npixels = width * height;
  guchar *buf, *p, *q, *endq;
  guchar *buf2 = NULL;  /* please the compiler */
  guchar runcount, byte;
  z_stream zstream;

  switch (ia->compression)
    {
    case PSP_COMP_NONE:
      if (bytespp == 1)
        {
          if ((width % 4) == 0)
            fread (pixels[0], height * width, 1, f);
          else
            {
              for (y = 0; y < height; y++)
                {
                  fread (pixels[y], width, 1, f);
                  fseek (f, 4 - (width % 4), SEEK_CUR);
                }
            }
        }
      else
        {
          buf = g_malloc (width);
          for (y = 0; y < height; y++)
            {
              fread (buf, width, 1, f);
              if (width % 4)
                fseek (f, 4 - (width % 4), SEEK_CUR);
              p = buf;
              q = pixels[y] + offset;
              for (i = 0; i < width; i++)
                {
                  *q = *p++;
                  q += bytespp;
                }
            }
          g_free (buf);
        }
      break;

    case PSP_COMP_RLE:
      q = pixels[0] + offset;
      endq = q + npixels * bytespp;
      buf = g_malloc (127);
      while (q < endq)
        {
          p = buf;
          fread (&runcount, 1, 1, f);
          if (runcount > 128)
            {
              runcount -= 128;
              fread (&byte, 1, 1, f);
              memset (buf, byte, runcount);
            }
          else
            fread (buf, runcount, 1, f);

          /* prevent buffer overflow for bogus data */
          runcount = MIN (runcount, endq - q);

          if (bytespp == 1)
            {
              memmove (q, buf, runcount);
              q += runcount;
            }
          else
            {
              p = buf;
              for (i = 0; i < runcount; i++)
                {
                  *q = *p++;
                  q += bytespp;
                }
            }
        }
      g_free (buf);
      break;

    case PSP_COMP_LZ77:
      buf = g_malloc (compressed_len);
      fread (buf, compressed_len, 1, f);
      zstream.next_in = buf;
      zstream.avail_in = compressed_len;
      zstream.zalloc = psp_zalloc;
      zstream.zfree = psp_zfree;
      zstream.opaque = f;
      if (inflateInit (&zstream) != Z_OK)
        {
          g_message ("zlib error");
          return -1;
        }
      if (bytespp == 1)
        zstream.next_out = pixels[0];
      else
        {
          buf2 = g_malloc (npixels);
          zstream.next_out = buf2;
        }
      zstream.avail_out = npixels;
      if (inflate (&zstream, Z_FINISH) != Z_STREAM_END)
        {
          g_message ("zlib error");
          inflateEnd (&zstream);
          return -1;
        }
      inflateEnd (&zstream);
      g_free (buf);

      if (bytespp > 1)
        {
          p = buf2;
          q = pixels[0] + offset;
          for (i = 0; i < npixels; i++)
            {
              *q = *p++;
              q += bytespp;
            }
          g_free (buf2);
        }
      break;
    }

  return 0;
}