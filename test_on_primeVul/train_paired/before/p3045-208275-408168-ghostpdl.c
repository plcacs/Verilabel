okiibm_print_page1(gx_device_printer *pdev, gp_file *prn_stream, int y_9pin_high,
  const char *init_string, int init_length,
  const char *end_string, int end_length)
{
        static const char graphics_modes_9[5] =
        {
        -1, 0 /*60*/, 1 /*120*/, -1, 3 /*240*/
        };

        int in_y_mult = (y_9pin_high ? 2 : 1);
        int line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
        /* Note that in_size is a multiple of 8. */
        int in_size = line_size * (8 * in_y_mult);
        byte *buf1 = (byte *)gs_malloc(pdev->memory, in_size, 1, "okiibm_print_page(buf1)");
        byte *buf2 = (byte *)gs_malloc(pdev->memory, in_size, 1, "okiibm_print_page(buf2)");
        byte *in = buf1;
        byte *out = buf2;
        int out_y_mult = 1;
        int x_dpi = pdev->x_pixels_per_inch;
        char start_graphics = graphics_modes_9[x_dpi / 60];
        int first_pass = (start_graphics == 3 ? 1 : 0);
        int last_pass = first_pass * 2;
        int y_passes = (y_9pin_high ? 2 : 1);
        int skip = 0, lnum = 0, pass, ypass;
        int y_step = 0;

        /* Check allocations */
        if ( buf1 == 0 || buf2 == 0 )
        {	if ( buf1 )
                  gs_free(pdev->memory, (char *)buf1, in_size, 1, "okiibm_print_page(buf1)");
                if ( buf2 )
                  gs_free(pdev->memory, (char *)buf2, in_size, 1, "okiibm_print_page(buf2)");
                return_error(gs_error_VMerror);
        }

        /* Initialize the printer. */
        gp_fwrite(init_string, 1, init_length, prn_stream);

        /* Print lines of graphics */
        while ( lnum < pdev->height )
        {
                byte *in_data;
                byte *inp;
                byte *in_end;
                byte *out_end = NULL;
                int lcnt;

                /* Copy 1 scan line and test for all zero. */
                gdev_prn_get_bits(pdev, lnum, in, &in_data);
                if ( in_data[0] == 0 &&
                     !memcmp((char *)in_data, (char *)in_data + 1, line_size - 1)
                   )
                {
                        lnum++;
                        skip += 2 / in_y_mult;
                        continue;
                }

                /*
                 * Vertical tab to the appropriate position.
                 * The skip count is in 1/144" steps.  If total
                 * vertical request is not a multiple od 1/72"
                 * we need to make sure the page is actually
                 * going to advance.
                 */
                if ( skip & 1 )
                {
                        int n = 1 + (y_step == 0 ? 1 : 0);
                        gp_fprintf(prn_stream, "\033J%c", n);
                        y_step = (y_step + n) % 3;
                        skip -= 1;
                }
                skip = skip / 2 * 3;
                while ( skip > 255 )
                {
                        gp_fputs("\033J\377", prn_stream);
                        skip -= 255;
                }
                if ( skip )
                {
                        gp_fprintf(prn_stream, "\033J%c", skip);
                }

                /* Copy the the scan lines. */
                lcnt = gdev_prn_copy_scan_lines(pdev, lnum, in, in_size);
                if ( lcnt < 8 * in_y_mult )
                {	/* Pad with lines of zeros. */
                        memset(in + lcnt * line_size, 0,
                               in_size - lcnt * line_size);
                }

                if ( y_9pin_high )
                {	/* Shuffle the scan lines */
                        byte *p;
                        int i;
                        static const char index[] =
                        {  0, 2, 4, 6, 8, 10, 12, 14,
                           1, 3, 5, 7, 9, 11, 13, 15
                        };
                        for ( i = 0; i < 16; i++ )
                        {
                                memcpy( out + (i * line_size),
                                        in + (index[i] * line_size),
                                        line_size);
                        }
                        p = in;
                        in = out;
                        out = p;
                }

        for ( ypass = 0; ypass < y_passes; ypass++ )
        {
            for ( pass = first_pass; pass <= last_pass; pass++ )
            {
                /* We have to 'transpose' blocks of 8 pixels x 8 lines, */
                /* because that's how the printer wants the data. */

                if ( pass == first_pass )
                {
                    out_end = out;
                    inp = in;
                    in_end = inp + line_size;

                    for ( ; inp < in_end; inp++, out_end += 8 )
                    {
                        gdev_prn_transpose_8x8(inp + (ypass * 8 * line_size),
                                               line_size, out_end, 1);
                    }
                    /* Remove trailing 0s. */
                    while ( out_end > out && out_end[-1] == 0 )
                    {
                        out_end--;
                    }
                }

                /* Transfer whatever is left and print. */
                if ( out_end > out )
                {
                    okiibm_output_run(out, (int)(out_end - out),
                                   out_y_mult, start_graphics,
                                   prn_stream, pass);
                }
                gp_fputc('\r', prn_stream);
            }
            if ( ypass < y_passes - 1 )
            {
                int n = 1 + (y_step == 0 ? 1 : 0);
                gp_fprintf(prn_stream, "\033J%c", n);
                y_step = (y_step + n) % 3;
            }
        }
        skip = 16 - y_passes + 1;		/* no skip on last Y pass */
        lnum += 8 * in_y_mult;
        }

        /* Reinitialize the printer. */
        gp_fwrite(end_string, 1, end_length, prn_stream);
        gp_fflush(prn_stream);

        gs_free(pdev->memory, (char *)buf2, in_size, 1, "okiibm_print_page(buf2)");
        gs_free(pdev->memory, (char *)buf1, in_size, 1, "okiibm_print_page(buf1)");
        return 0;
}