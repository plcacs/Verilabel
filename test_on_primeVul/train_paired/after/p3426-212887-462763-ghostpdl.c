epsc_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{
    static int graphics_modes_9[5] = { -1, 0 /*60 */ , 1 /*120 */ , -1, DD + 3  /*240 */
    };
    static int graphics_modes_24[7] =
        { -1, 32 /*60 */ , 33 /*120 */ , 39 /*180 */ ,
        -1, -1, DD + 40         /*360 */
    };
    int y_24pin = pdev->y_pixels_per_inch > 72;
    int y_mult = (y_24pin ? 3 : 1);
    int line_size = (pdev->width + 7) >> 3;     /* always mono */
    int in_size = line_size * (8 * y_mult);
    int out_size = ((pdev->width + 7) & -8) * y_mult;
    byte *in;
    byte *out;
    int x_dpi = (int)pdev->x_pixels_per_inch;

    char start_graphics;
    int first_pass;
    int last_pass;
    int dots_per_space; 
    int bytes_per_space;
    int skip = 0, lnum = 0, pass;

    byte *color_in;
    int color_line_size, color_in_size;
    int spare_bits;
    int whole_bits;

    int max_dpi = 60 * (
            (y_24pin) ?
            sizeof(graphics_modes_24) / sizeof(graphics_modes_24[0])
            :
            sizeof(graphics_modes_9) / sizeof(graphics_modes_9[0])
            )
            - 1;
    if (x_dpi > max_dpi) {
        return_error(gs_error_rangecheck);
    }
    
    in =
        (byte *) gs_malloc(pdev->memory, in_size + 1, 1,
                           "epsc_print_page(in)");
    out =
        (byte *) gs_malloc(pdev->memory, out_size + 1, 1,
                           "epsc_print_page(out)");

    start_graphics = (char)
        ((y_24pin ? graphics_modes_24 : graphics_modes_9)[x_dpi / 60]);
    first_pass = (start_graphics & DD ? 1 : 0);
    last_pass = first_pass * 2;
    dots_per_space = x_dpi / 10;    /* pica space = 1/10" */
    bytes_per_space = dots_per_space * y_mult;

    /* declare color buffer and related vars */
    spare_bits = (pdev->width % 8); /* left over bits to go to margin */
    whole_bits = pdev->width - spare_bits;

    /* Check allocations */
    if (in == 0 || out == 0) {
        if (in)
            gs_free(pdev->memory, (char *)in, in_size + 1, 1,
                    "epsc_print_page(in)");
        if (out)
            gs_free(pdev->memory, (char *)out, out_size + 1, 1,
                    "epsc_print_page(out)");
        return -1;
    }

    /* Initialize the printer and reset the margins. */
    gp_fwrite("\033@\033P\033l\000\033Q\377\033U\001\r", 1, 14, prn_stream);

    /* Create color buffer */
    if (gx_device_has_color(pdev)) {
        color_line_size = gdev_mem_bytes_per_scan_line((gx_device *) pdev);
        color_in_size = color_line_size * (8 * y_mult);
        if ((color_in = (byte *) gs_malloc(pdev->memory, color_in_size + 1, 1,
                                           "epsc_print_page(color)")) == 0) {
            gs_free(pdev->memory, (char *)in, in_size + 1, 1,
                    "epsc_print_page(in)");
            gs_free(pdev->memory, (char *)out, out_size + 1, 1,
                    "epsc_print_page(out)");
            return (-1);
        }
    } else {
        color_in = in;
        color_in_size = in_size;
        color_line_size = line_size;
    }

    /* Print lines of graphics */
    while (lnum < pdev->height) {
        int lcnt;
        byte *nextcolor = NULL; /* position where next color appears */
        byte *nextmono = NULL;  /* position to map next color */

        /* Copy 1 scan line and test for all zero. */
        gdev_prn_copy_scan_lines(pdev, lnum, color_in, color_line_size);

        if (color_in[0] == 0 &&
            !memcmp((char *)color_in, (char *)color_in + 1,
                    color_line_size - 1)
            ) {
            lnum++;
            skip += 3 / y_mult;
            continue;
        }

        /* Vertical tab to the appropriate position. */
        while (skip > 255) {
            gp_fputs("\033J\377", prn_stream);
            skip -= 255;
        }
        if (skip)
            gp_fprintf(prn_stream, "\033J%c", skip);

        /* Copy the rest of the scan lines. */
        lcnt = 1 + gdev_prn_copy_scan_lines(pdev, lnum + 1,
                                            color_in + color_line_size,
                                            color_in_size - color_line_size);

        if (lcnt < 8 * y_mult) {
            memset((char *)(color_in + lcnt * color_line_size), 0,
                   color_in_size - lcnt * color_line_size);
            if (gx_device_has_color(pdev))      /* clear the work buffer */
                memset((char *)(in + lcnt * line_size), 0,
                       in_size - lcnt * line_size);
        }

        /*
        ** We need to create a normal epson scan line from our color scan line
        ** We do this by setting a bit in the "in" buffer if the pixel byte is set
        ** to any color.  We then search for any more pixels of that color, setting
        ** "in" accordingly.  If any other color is found, we save it for the next
        ** pass.  There may be up to 7 passes.
        ** In the future, we should make the passes so as to maximize the
        ** life of the color ribbon (i.e. go lightest to darkest).
        */
        do {
            byte *inp = in;
            byte *in_end = in + line_size;
            byte *out_end = out;
            byte *out_blk;
            register byte *outp;

            if (gx_device_has_color(pdev)) {
                register int i, j;
                register byte *outbuf, *realbuf;
                byte current_color;
                int end_next_bits = whole_bits;
                int lastbits;

                /* Move to the point in the scanline that has a new color */
                if (nextcolor) {
                    realbuf = nextcolor;
                    outbuf = nextmono;
                    memset((char *)in, 0, (nextmono - in));
                    i = nextcolor - color_in;
                    nextcolor = NULL;
                    end_next_bits = (i / color_line_size) * color_line_size
                        + whole_bits;
                } else {
                    i = 0;
                    realbuf = color_in;
                    outbuf = in;
                    nextcolor = NULL;
                }
                /* move thru the color buffer, turning on the appropriate
                ** bit in the "mono" buffer", setting pointers to the next
                ** color and changing the color output of the epson
                */
                for (current_color = 0; i <= color_in_size && outbuf < in + in_size; outbuf++) {
                    /* Remember, line_size is rounded up to next whole byte
                    ** whereas color_line_size is the proper length
                    ** We only want to set the proper bits in the last line_size byte.
                    */
                    if (spare_bits && i == end_next_bits) {
                        end_next_bits = whole_bits + i + spare_bits;
                        lastbits = 8 - spare_bits;
                    } else
                        lastbits = 0;

                    for (*outbuf = 0, j = 8;
                         --j >= lastbits && i <= color_in_size;
                         realbuf++, i++) {
                        if (*realbuf) {
                            if (current_color > 0) {
                                if (*realbuf == current_color) {
                                    *outbuf |= 1 << j;
                                    *realbuf = 0;       /* throw this byte away */
                                }
                                /* save this location for next pass */
                                else if (nextcolor == NULL) {
                                    nextcolor = realbuf - (7 - j);
                                    nextmono = outbuf;
                                }
                            } else {
                                *outbuf |= 1 << j;
                                current_color = *realbuf;       /* set color */
                                *realbuf = 0;
                            }
                        }
                    }
                }
                *outbuf = 0;    /* zero the end, for safe keeping */
               /* Change color on the EPSON, current_color must be set
               ** but lets check anyway
               */
                if (current_color)
                    gp_fprintf(prn_stream, "\033r%c", current_color ^ 7);
            }

            /* We have to 'transpose' blocks of 8 pixels x 8 lines, */
            /* because that's how the printer wants the data. */
            /* If we are in a 24-pin mode, we have to transpose */
            /* groups of 3 lines at a time. */

            if (y_24pin) {
                for (; inp < in_end; inp++, out_end += 24) {
                    gdev_prn_transpose_8x8(inp, line_size, out_end, 3);
                    gdev_prn_transpose_8x8(inp + line_size * 8, line_size,
                                           out_end + 1, 3);
                    gdev_prn_transpose_8x8(inp + line_size * 16, line_size,
                                           out_end + 2, 3);
                }
                /* Remove trailing 0s. */
                while (out_end > out && out_end[-1] == 0 &&
                       out_end[-2] == 0 && out_end[-3] == 0)
                    out_end -= 3;
            } else {
                for (; inp < in_end; inp++, out_end += 8) {
                    gdev_prn_transpose_8x8(inp, line_size, out_end, 1);
                }
                /* Remove trailing 0s. */
                while (out_end > out && out_end[-1] == 0)
                    out_end--;
            }

            for (pass = first_pass; pass <= last_pass; pass++) {
                for (out_blk = outp = out; outp < out_end;) {   /* Skip a run of leading 0s. */
                    /* At least 10 are needed to make tabbing worth it. */
                    /* We do everything by 3's to avoid having to make */
                    /* different cases for 9- and 24-pin. */

                    if (*outp == 0 && outp + 12 <= out_end &&
                        outp[1] == 0 && outp[2] == 0 &&
                        (outp[3] | outp[4] | outp[5]) == 0 &&
                        (outp[6] | outp[7] | outp[8]) == 0 &&
                        (outp[9] | outp[10] | outp[11]) == 0) {
                        byte *zp = outp;
                        int tpos;
                        byte *newp;

                        outp += 12;
                        while (outp + 3 <= out_end && *outp == 0 &&
                               outp[1] == 0 && outp[2] == 0)
                            outp += 3;
                        tpos = (outp - out) / bytes_per_space;
                        newp = out + tpos * bytes_per_space;
                        if (newp > zp + 10) {   /* Output preceding bit data. */
                            if (zp > out_blk)
                                /* only false at */
                                /* beginning of line */
                                epsc_output_run(out_blk, (int)(zp - out_blk),
                                                y_mult, start_graphics,
                                                prn_stream, pass);
                            /* Tab over to the appropriate position. */
                            gp_fprintf(prn_stream, "\033D%c%c\t", tpos, 0);
                            out_blk = outp = newp;
                        }
                    } else
                        outp += y_mult;
                }
                if (outp > out_blk)
                    epsc_output_run(out_blk, (int)(outp - out_blk),
                                    y_mult, start_graphics, prn_stream, pass);

                gp_fputc('\r', prn_stream);
            }
        } while (nextcolor);
        skip = 24;
        lnum += 8 * y_mult;
    }

    /* Eject the page and reinitialize the printer */
    gp_fputs("\f\033@", prn_stream);

    gs_free(pdev->memory, (char *)out, out_size + 1, 1,
            "epsc_print_page(out)");
    gs_free(pdev->memory, (char *)in, in_size + 1, 1, "epsc_print_page(in)");
    if (gx_device_has_color(pdev))
        gs_free(pdev->memory, (char *)color_in, color_in_size + 1, 1,
                "epsc_print_page(rin)");
    return 0;
}