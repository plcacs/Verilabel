jetp3852_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
#define DATA_SIZE (LINE_SIZE * 8)

    unsigned int cnt_2prn;
    unsigned int count,tempcnt;
    unsigned char vtp,cntc1,cntc2;
    int line_size_color_plane;

    byte data[DATA_SIZE];
    byte plane_data[LINE_SIZE * 3];

    /* Set initial condition for printer */
    gp_fputs("\033@",prn_stream);

    /* Send each scan line in turn */
    {
        int lnum;
        int line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
        int num_blank_lines = 0;

        if (line_size > DATA_SIZE) {
            emprintf2(pdev->memory, "invalid resolution and/or width gives line_size = %d, max. is %d\n",
                      line_size, DATA_SIZE);
            return_error(gs_error_rangecheck);
        }

        for ( lnum = 0; lnum < pdev->height; lnum++ ) {
            byte *end_data = data + line_size;
            gdev_prn_copy_scan_lines(pdev, lnum,
                                     (byte *)data, line_size);
            /* Remove trailing 0s. */
            while ( end_data > data && end_data[-1] == 0 )
                end_data--;
            if ( end_data == data ) {
                /* Blank line */
                num_blank_lines++;
            } else {
                int i;
                byte *odp;
                byte *row;

                /* Pad with 0s to fill out the last */
                /* block of 8 bytes. */
                memset(end_data, 0, 7);

                /* Transpose the data to get pixel planes. */
                for ( i = 0, odp = plane_data; i < DATA_SIZE;
                      i += 8, odp++
                    ) { /* The following is for 16-bit machines */
#define spread3(c)\
 { 0, c, c*0x100, c*0x101, c*0x10000L, c*0x10001L, c*0x10100L, c*0x10101L }
                    static ulong spr40[8] = spread3(0x40);
                    static ulong spr8[8] = spread3(8);
                    static ulong spr2[8] = spread3(2);
                    register byte *dp = data + i;
                    register ulong pword =
                                     (spr40[dp[0]] << 1) +
                                     (spr40[dp[1]]) +
                                     (spr40[dp[2]] >> 1) +
                                     (spr8[dp[3]] << 1) +
                                     (spr8[dp[4]]) +
                                     (spr8[dp[5]] >> 1) +
                                     (spr2[dp[6]]) +
                                     (spr2[dp[7]] >> 1);
                    odp[0] = (byte)(pword >> 16);
                    odp[LINE_SIZE] = (byte)(pword >> 8);
                    odp[LINE_SIZE*2] = (byte)(pword);
                }
                /* Skip blank lines if any */
                if ( num_blank_lines > 0 ) {
                    /* Do "dot skips" */
                    while(num_blank_lines > 255) {
                        gp_fputs("\033e\377",prn_stream);
                        num_blank_lines -= 255;
                    }
                    vtp = num_blank_lines;
                    gp_fprintf(prn_stream,"\033e%c",vtp);
                    num_blank_lines = 0;
                }

                /* Transfer raster graphics in the order R, G, B. */
                /* Apparently it is stored in B, G, R */
                /* Calculate the amount of data to send by what */
                /* Ghostscript tells us the scan line_size in (bytes) */

                count = line_size / 3;
                line_size_color_plane = count / 3;
                cnt_2prn = line_size_color_plane * 3 + 5;
                tempcnt = cnt_2prn;
                cntc1 = (tempcnt & 0xFF00) >> 8;
                cntc2 = (tempcnt & 0x00FF);
                gp_fprintf(prn_stream, "\033[O%c%c\200\037",cntc2,cntc1);
                gp_fputc('\000',prn_stream);
                gp_fputs("\124\124",prn_stream);

                for ( row = plane_data + LINE_SIZE * 2, i = 0;
                      i < 3; row -= LINE_SIZE, i++ ) {
                    int jj;
                    byte ctemp;
                    odp = row;
                    /* Complement bytes */
                    for (jj=0; jj< line_size_color_plane; jj++) {
                        ctemp = *odp;
                        *odp++ = ~ctemp;
                    }
                    gp_fwrite(row, sizeof(byte),
                              line_size_color_plane, prn_stream);
                }
            }
        }
    }

    /* eject page */
    gp_fputs("\014", prn_stream);

    return 0;
}