dot24_print_page (gx_device_printer *pdev, gp_file *prn_stream, char *init_string, int init_len)
{
  int xres = (int)pdev->x_pixels_per_inch;
  int yres = (int)pdev->y_pixels_per_inch;
  int x_high = (xres == 360);
  int y_high = (yres == 360);
  int bits_per_column = (y_high ? 48 : 24);
  uint line_size = gdev_prn_raster (pdev);
  uint in_size = line_size * bits_per_column;
  byte *in = (byte *) gs_malloc (pdev->memory, in_size, 1, "dot24_print_page (in)");
  uint out_size = ((pdev->width + 7) & -8) * 3;
  byte *out = (byte *) gs_malloc (pdev->memory, out_size, 1, "dot24_print_page (out)");
  int y_passes = (y_high ? 2 : 1);
  int dots_per_space = xres / 10;	/* pica space = 1/10" */
  int bytes_per_space = dots_per_space * 3;
  int skip = 0, lnum = 0, ypass;

  /* Check allocations */
  if (in == 0 || out == 0)
    {
      if (out)
        gs_free (pdev->memory, (char *) out, out_size, 1, "dot24_print_page (out)");
      if (in)
        gs_free (pdev->memory, (char *) in, in_size, 1, "dot24_print_page (in)");
      return_error (gs_error_VMerror);
    }

  /* Initialize the printer and reset the margins. */
  gp_fwrite (init_string, init_len - 1, sizeof (char), prn_stream);
  gp_fputc ((int) (pdev->width / pdev->x_pixels_per_inch * 10) + 2,
         prn_stream);

  /* Print lines of graphics */
  while (lnum < pdev->height)
    {
      byte *inp;
      byte *in_end;
      byte *out_end;
      byte *out_blk;
      register byte *outp;
      int lcnt;

      /* Copy 1 scan line and test for all zero. */
      gdev_prn_copy_scan_lines (pdev, lnum, in, line_size);
      if (in[0] == 0
          && !memcmp ((char *) in, (char *) in + 1, line_size - 1))
        {
          lnum++;
          skip += 2 - y_high;
          continue;
        }

      /* Vertical tab to the appropriate position. */
      while ((skip >> 1) > 255)
        {
          gp_fputs ("\033J\377", prn_stream);
          skip -= 255 * 2;
        }

      if (skip)
        {
          if (skip >> 1)
            gp_fprintf (prn_stream, "\033J%c", skip >> 1);
          if (skip & 1)
            gp_fputc ('\n', prn_stream);
        }

      /* Copy the rest of the scan lines. */
      if (y_high)
        {
          inp = in + line_size;
          for (lcnt = 1; lcnt < 24; lcnt++, inp += line_size)
            if (!gdev_prn_copy_scan_lines (pdev, lnum + lcnt * 2, inp,
                                           line_size))
              {
                memset (inp, 0, (24 - lcnt) * line_size);
                break;
              }
          inp = in + line_size * 24;
          for (lcnt = 0; lcnt < 24; lcnt++, inp += line_size)
            if (!gdev_prn_copy_scan_lines (pdev, lnum + lcnt * 2 + 1, inp,
                                           line_size))
              {
                memset (inp, 0, (24 - lcnt) * line_size);
                break;
              }
        }
      else
        {
          lcnt = 1 + gdev_prn_copy_scan_lines (pdev, lnum + 1, in + line_size,
                                               in_size - line_size);
          if (lcnt < 24)
            /* Pad with lines of zeros. */
            memset (in + lcnt * line_size, 0, in_size - lcnt * line_size);
        }

      for (ypass = 0; ypass < y_passes; ypass++)
        {
          out_end = out;
          inp = in;
          if (ypass)
            inp += line_size * 24;
          in_end = inp + line_size;

          for (; inp < in_end; inp++, out_end += 24)
            {
              memflip8x8 (inp, line_size, out_end, 3);
              memflip8x8 (inp + line_size * 8, line_size, out_end + 1, 3);
              memflip8x8 (inp + line_size * 16, line_size, out_end + 2, 3);
            }
          /* Remove trailing 0s. */
          while (out_end - 3 >= out && out_end[-1] == 0
                 && out_end[-2] == 0 && out_end[-3] == 0)
            out_end -= 3;

          for (out_blk = outp = out; outp < out_end;)
            {
              /* Skip a run of leading 0s. */
              /* At least 10 are needed to make tabbing worth it. */

              if (outp[0] == 0 && outp + 12 <= out_end
                  && outp[1] == 0 && outp[2] == 0
                  && outp[3] == 0 && outp[4] == 0 && outp[5] == 0
                  && outp[6] == 0 && outp[7] == 0 && outp[8] == 0
                  && outp[9] == 0 && outp[10] == 0 && outp[11] == 0)
                {
                  byte *zp = outp;
                  int tpos;
                  byte *newp;
                  outp += 12;
                  while (outp + 3 <= out_end
                         && outp[0] == 0 && outp[1] == 0 && outp[2] == 0)
                    outp += 3;
                  tpos = (outp - out) / bytes_per_space;
                  newp = out + tpos * bytes_per_space;
                  if (newp > zp + 10)
                    {
                      /* Output preceding bit data. */
                      /* only false at beginning of line */
                      if (zp > out_blk)
                        {
                          if (x_high)
                            dot24_improve_bitmap (out_blk, (int) (zp - out_blk));
                          dot24_output_run (out_blk, (int) (zp - out_blk),
                                          x_high, prn_stream);
                        }
                      /* Tab over to the appropriate position. */
                      gp_fprintf (prn_stream, "\033D%c%c\t", tpos, 0);
                      out_blk = outp = newp;
                    }
                }
              else
                outp += 3;
            }
          if (outp > out_blk)
            {
              if (x_high)
                dot24_improve_bitmap (out_blk, (int) (outp - out_blk));
              dot24_output_run (out_blk, (int) (outp - out_blk), x_high,
                              prn_stream);
            }

          gp_fputc ('\r', prn_stream);
          if (ypass < y_passes - 1)
            gp_fputc ('\n', prn_stream);
        }
      skip = 48 - y_high;
      lnum += bits_per_column;
    }

  /* Eject the page and reinitialize the printer */
  gp_fputs ("\f\033@", prn_stream);
  gp_fflush (prn_stream);

  gs_free (pdev->memory, (char *) out, out_size, 1, "dot24_print_page (out)");
  gs_free (pdev->memory, (char *) in, in_size, 1, "dot24_print_page (in)");

  return 0;
}