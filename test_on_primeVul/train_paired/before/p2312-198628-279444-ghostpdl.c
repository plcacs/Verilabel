/* ------ Driver procedures ------ */

/* Send the page to the printer. */
static int
lxm5700m_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
    int lnum,minX, maxX, i, l, highestX, leastX, extent;
    int direction = RIGHTWARD;
    int lastY = 0;

    int line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
    /* Note that in_size is a multiple of 8. */
    int in_size = line_size * (swipeHeight);
    int swipeBuf_size = in_size;
    byte *buf1 = (byte *)gs_malloc(pdev->memory, in_size, 1, "lxm_print_page(buf1)");
    byte *swipeBuf =
        (byte *)gs_malloc(pdev->memory, swipeBuf_size, 1, "lxm_print_page(swipeBuf)");
    byte *in = buf1;

    /* Check allocations */
    if ( buf1 == 0 || swipeBuf == 0 ) {
        if ( buf1 )
quit_ignomiously: /* and a goto into an if statement is pretty ignomious! */
        gs_free(pdev->memory, (char *)buf1, in_size, 1, "lxm_print_page(buf1)");
        if ( swipeBuf )
            gs_free(pdev->memory, (char *)swipeBuf, swipeBuf_size, 1, "lxm_print_page(swipeBuf)");
        return_error(gs_error_VMerror);
    }

    {	/* Initialize the printer and reset the margins. */
        static const char init_string[] = {
            init1(),
            init2(),
            init3()
        };
        gp_fwrite(init_string, 1, sizeof(init_string), prn_stream);
    }
    /* Print lines of graphics */
    for (lnum=0; lnum < pdev->height-swipeHeight ; ) { /* increment in body */
        byte *in_data;
        register byte *outp;
        int lcnt;

        {	/* test for blank scan lines.  We  maintain the */
            /* loop invariant lnum <pdev->height, but modify lnum */
            int l;

            for (l=lnum; l<pdev->height; l++) {
                /* Copy 1 scan line and test for all zero. */
                gdev_prn_get_bits(pdev, l, in, &in_data);
                if ( in_data[0] != 0 ||
                     memcmp((char *)in_data, (char *)in_data + 1, line_size - 1)
                     ) {
                    break;
                }
            }/* end for l */

            /* now l is the next non-blank scan line */
            if (l >= pdev->height) {/* if there are no more bits on this page */
                lnum = l;
                break;			/* end the loop and eject the page*/
            }

            /* leave room for following swipe to reinforce these bits */
            if (l-lnum > overLap) lnum = l - overLap;

            /* if the first non-blank near bottom of page */
            if (lnum >=pdev->height - swipeHeight) {
                /* don't move the printhead over empty air*/
                lnum = pdev->height - swipeHeight;
            }
        }

        /* Copy the the scan lines. */
        lcnt = gdev_prn_copy_scan_lines(pdev, lnum, in, in_size);
        if ( lcnt < swipeHeight ) {
            /* Pad with lines of zeros. */
            memset(in + lcnt * line_size, 0,
                   in_size - lcnt * line_size);
        }

        /* compute right and left margin for this swipe */
        minX = line_size;
        maxX = 0;
        for (l=0; l<swipeHeight; l++) {/* for each line of swipe */
            for (i=0; i<minX; i++) {/* look for left-most non-zero byte*/
                if (in[l*line_size+i] !=0) {
                    minX = i;
                    break;
                }
            }
            for (i=line_size-1; i>=maxX; i--) {/* look for right-most */
                if (in[l*line_size+i] !=0) {
                    maxX = i;
                    break;
                }
            }
        }
        minX = (minX&(-2)); /* truncate to even */
        maxX = (maxX+3)&-2; /* raise to even */

        highestX = maxX*8-1;
        leastX = minX*8;
        extent = highestX -leastX +1;

        outp = swipeBuf;

        /* macro, not fcn call.  Space penalty is modest, speed helps */
#define buffer_store(x) if(outp-swipeBuf>=swipeBuf_size) {\
            gs_free(pdev->memory, (char *)swipeBuf, swipeBuf_size, 1, "lxm_print_page(swipeBuf)");\
            swipeBuf_size*=2;\
            swipeBuf = (byte *)gs_malloc(pdev->memory, swipeBuf_size, 1, "lxm_print_page(swipeBuf)");\
            if (swipeBuf == 0) goto quit_ignomiously;\
            break;}\
        else *outp++ = (x)

            {/* work out the bytes to store for this swipe*/

                int sx, sxBy8, sxMask;
                int words[directorySize];
                bool f, sum;
                int retval=0;
                int j,c,y;
                int j1,c1;
                int i,b,x, directory ;

                /* want to set up pointers for (upto two) stripes covered by the output*/

                /* now for each column covered by output: */
                for (x=leastX; x<=highestX; x++) {
                    for (i=0; i<directorySize; i++) {
                        words[i] = 0;
                    }
                    directory = 0x2000;	/* empty directory != 0 */

                    /* prime loops: make comparisons here */
                    switch (direction) {
                    case(RIGHTWARD):
                        sx = (x&1)==1 ? x : x-(((lxm_device*)pdev)->headSeparation);
                        j1 = (x&1);	/* even if x even, odd if x odd */
                        break;
                    default:	/* shouldn't happen ... but compilation checks */
                    case(LEFTWARD):
                        sx = (x&1)==0 ? x : x-((lxm_device*)pdev)->headSeparation;
                        j1 = 1-(x&1);	/* odd if x even, even if x odd */
                    }
                    c1 = 0x8000 >> j1;

                    sxBy8 = sx/8;
                    sxMask = 0x80>>(sx%8);

                    /* loop through all the swipeHeight bits of this column */
                    for (i = 0, b=1, y= sxBy8+j1*line_size; i < directorySize; i++,b<<=1) {
                        sum = false;
                        for (j=j1,c=c1 /*,y=i*16*line_size+sxBy8*/; j<16; j+=2, y+=2*line_size, c>>=2) {
                            f = (in[y]&sxMask);
                            if (f) {
                                words[i] |= c;
                                sum |= f;
                            }
                        }
                        if (!sum) directory |=b;
                    }
                    retval+=2;
                    buffer_store(directory>>8); buffer_store(directory&0xff);
                    if (directory != 0x3fff) {
                        for (i=0; i<directorySize; i++) {
                            if (words[i] !=0) {
                                buffer_store(words[i]>>8) ; buffer_store(words[i]&0xff);
                                retval += 2;
                            }
                        }
                    }
                }
#undef buffer_store
            }
        {/* now write out header, then buffered bits */
            int leastY = lnum;

            /* compute size of swipe, needed for header */
            int sz = 0x1a + outp - swipeBuf;

            /* put out header*/
            int deltaY = 2*(leastY - lastY);  /* vert coordinates here are 1200 dpi */
            lastY = leastY;
            outByte(0x1b); outByte('*'); outByte(3);
            outByte(deltaY>>8); outByte(deltaY&0xff);
            outByte(0x1b); outByte('*'); outByte(4); outByte(0); outByte(0);
            outByte(sz>>8); outByte(sz&0xff); outByte(0); outByte(3);
            outByte(1); outByte(1); outByte(0x1a);
            outByte(0);
            outByte(extent>>8); outByte(extent&0xff);
            outByte(leastX>>8); outByte(leastX&0xff);
            outByte(highestX>>8); outByte(highestX&0xff);
            outByte(0); outByte(0);
            outByte(0x22); outByte(0x33); outByte(0x44);
            outByte(0x55); outByte(1);
            /* put out bytes */
            gp_fwrite(swipeBuf,1,outp-swipeBuf,prn_stream);
        }
            lnum += overLap;
            direction ^= 1;
    }/* ends the loop for swipes of the print head.*/

    /* Eject the page and reinitialize the printer */
    {
        static const char bottom[] = {
            fin() /*,  looks like I can get away with only this much ...
            init1(),
            init3(),
            fin()   ,
            top(),
            fin()  */
        };
        gp_fwrite(bottom, 1, sizeof(bottom), prn_stream);
    }
    gp_fflush(prn_stream);
