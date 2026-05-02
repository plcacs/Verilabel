image_render_color_thresh(gx_image_enum *penum_orig, const byte *buffer, int data_x,
                          uint w, int h, gx_device * dev)
{
    gx_image_enum *penum = penum_orig; /* const within proc */
    image_posture posture = penum->posture;
    int vdi;  /* amounts to replicate */
    fixed xrun = 0;
    byte *thresh_align;
    byte *devc_contone[GX_DEVICE_COLOR_MAX_COMPONENTS];
    byte *psrc_plane[GX_DEVICE_COLOR_MAX_COMPONENTS];
    byte *devc_contone_gray;
    const byte *psrc = buffer + data_x;
    int dest_width, dest_height, data_length;
    int spp_out = dev->color_info.num_components;
    int position, i, j, k;
    int offset_bits = penum->ht_offset_bits;
    int contone_stride = 0;  /* Not used in landscape case */
    fixed offset;
    int src_size;
    bool flush_buff = false;
    byte *psrc_temp;
    int offset_contone[GX_DEVICE_COLOR_MAX_COMPONENTS];    /* to ensure 128 bit boundary */
    int offset_threshold;  /* to ensure 128 bit boundary */
    gx_dda_fixed dda_ht;
    int xn, xr;		/* destination position (pixel, not contone buffer offset) */
    int code = 0;
    int spp_cm = 0;
    byte *psrc_cm = NULL, *psrc_cm_start = NULL;
    byte *bufend = NULL;
    int psrc_planestride = w/penum->spp;

    if (h != 0 && penum->line_size != 0) {      /* line_size == 0, nothing to do */
        /* Get the buffer into the device color space */
        code = image_color_icc_prep(penum, psrc, w, dev, &spp_cm, &psrc_cm,
                                    &psrc_cm_start,  &bufend, true);
        if (code < 0)
            return code;
    } else {
        if (penum->ht_landscape.count == 0 || posture == image_portrait) {
            return 0;
        } else {
            /* Need to flush the buffer */
            offset_bits = penum->ht_landscape.count;
            penum->ht_offset_bits = offset_bits;
            penum->ht_landscape.offset_set = true;
            flush_buff = true;
        }
    }
    /* Data is now in the proper destination color space.  Now we want
       to go ahead and get the data into the proper spatial setting and then
       threshold.  First get the data spatially sampled correctly */
    src_size = penum->rect.w;

    /* Set up the dda.  We could move this out but the cost is pretty small */
    dda_ht = (posture == image_portrait) ? penum->dda.pixel0.x : penum->dda.pixel0.y;
    if (penum->dxx > 0)
        dda_translate(dda_ht, -fixed_epsilon);      /* to match rounding in non-fast code */

    switch (posture) {
        case image_portrait:
            /* Figure out our offset in the contone and threshold data
               buffers so that we ensure that we are on the 128bit
               memory boundaries when we get offset_bits into the data. */
            /* Can't do this earlier, as GC might move the buffers. */
            xrun = dda_current(dda_ht);
            dest_width = gxht_dda_length(&dda_ht, src_size);
            if (penum->x_extent.x < 0)
                xrun += penum->x_extent.x;
            vdi = penum->hci;
            contone_stride = penum->line_size;
            offset_threshold = (- (((long)(penum->thresh_buffer)) +
                                      penum->ht_offset_bits)) & 15;
            for (k = 0; k < spp_out; k ++) {
                offset_contone[k]   = (- (((long)(penum->line)) +
                                          contone_stride * k +
                                          penum->ht_offset_bits)) & 15;
            }
            data_length = dest_width;
            dest_height = fixed2int_var_rounded(any_abs(penum->y_extent.y));
#ifdef DEBUG
            /* Help in spotting problems */
            memset(penum->ht_buffer, 0x00, penum->ht_stride * vdi * spp_out);
#endif
            break;
        case image_landscape:
        default:
            /* Figure out our offset in the contone and threshold data buffers
               so that we ensure that we are on the 128bit memory boundaries.
               Can't do this earlier as GC may move the buffers.
             */
            vdi = penum->wci;
            contone_stride = penum->line_size;
            dest_width = fixed2int_var_rounded(any_abs(penum->y_extent.x));
            /* match height in gxht_thresh.c dev_width calculation */
            xrun = dda_current(dda_ht);            /* really yrun, but just used here for landscape */
            dest_height = gxht_dda_length(&dda_ht, src_size);
            data_length = dest_height;
            offset_threshold = (-(long)(penum->thresh_buffer)) & 15;
            for (k = 0; k < spp_out; k ++) {
                offset_contone[k]   = (- ((long)(penum->line) +
                                          contone_stride * k)) & 15;
            }
            /* In the landscaped case, we want to accumulate multiple columns
               of data before sending to the device.  We want to have a full
               byte of HT data in one write.  This may not be possible at the
               left or right and for those and for those we have so send partial
               chunks */
            /* Initialize our xstart and compute our partial bit chunk so
               that we get in sync with the 1 bit mem device 16 bit positions
               for the rest of the chunks */
            if (penum->ht_landscape.count == 0) {
                /* In the landscape case, the size depends upon
                   if we are moving left to right or right to left with
                   the image data.  This offset is to ensure that we  get
                   aligned in our chunks along 16 bit boundaries */
                penum->ht_landscape.offset_set = true;
                if (penum->ht_landscape.index < 0) {
                    penum->ht_landscape.xstart = penum->xci + vdi - 1;
                    offset_bits = (penum->ht_landscape.xstart % 16) + 1;
                    /* xci can be negative, so allow for that */
                    if (offset_bits <= 0) offset_bits += 16;
                } else {
                    penum->ht_landscape.xstart = penum->xci;
                    /* xci can be negative, see Bug 692569. */
                    offset_bits = 16 - penum->xci % 16;
                    if (offset_bits >= 16) offset_bits -= 16;
                }
                if (offset_bits == 0 || offset_bits == 16) {
                    penum->ht_landscape.offset_set = false;
                    penum->ht_offset_bits = 0;
                } else {
                    penum->ht_offset_bits = offset_bits;
                }
            }
            break;
    }
    if (flush_buff)
        goto flush;  /* All done */

    /* Get the pointers to our buffers */
    for (k = 0; k < spp_out; k++) {
        if (posture == image_portrait) {
            devc_contone[k] = penum->line + contone_stride * k +
                              offset_contone[k];
        } else {
            devc_contone[k] = penum->line + offset_contone[k] +
                              LAND_BITS * k * contone_stride;
        }
        psrc_plane[k] = psrc_cm + psrc_planestride * k;
    }
    xr = fixed2int_var_rounded(dda_current(dda_ht));	/* indexes in the destination (contone) */

    /* Do conversion to device resolution in quick small loops. */
    /* For now we have 3 cases.  A CMYK (4 channel), gray, or other case
       the latter of which is not yet implemented */
    switch (spp_out)
    {
        /* Monochrome output case */
        case 1:
            devc_contone_gray = devc_contone[0];
            switch (posture) {
                /* Monochrome portrait */
                case image_portrait:
                    if (penum->dst_width > 0) {
                        if (src_size == dest_width) {
                            memcpy(devc_contone_gray, psrc_cm, data_length);
                        } else if (src_size * 2 == dest_width) {
                            psrc_temp = psrc_cm;
                            for (k = 0; k < data_length; k+=2,
                                 devc_contone_gray+=2, psrc_temp++) {
                                *devc_contone_gray =
                                    *(devc_contone_gray+1) = *psrc_temp;
                            }
                        } else {
                        /* Mono case, forward */
                        psrc_temp = psrc_cm;
                        for (k=0; k<src_size; k++) {
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr < xn) {
                                *devc_contone_gray++ = *psrc_temp;
                                xr++;
                            }           /* at loop exit xn will be >= xr */
                            psrc_temp++;
                            }
                        }
                    } else {
                        /* Mono case, backwards */
                        devc_contone_gray += (data_length - 1);
                        psrc_temp = psrc_cm;
                        for (k=0; k<src_size; k++) {
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr > xn) {
                                *devc_contone_gray-- = *psrc_temp;
                                xr--;
                            }           /* at loop exit xn will be >= xr */
                            psrc_temp++;
                            }
                    }
                    break;
                /* Monochrome landscape */
                case image_landscape:
                    /* We store the data at this point into a column. Depending
                       upon our landscape direction we may be going left to right
                       or right to left. */
                    if (penum->ht_landscape.flipy) {
                        position = penum->ht_landscape.curr_pos +
                                    LAND_BITS * (data_length - 1);
                        psrc_temp = psrc_cm;
                        for (k=0; k<src_size; k++) {
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr > xn) {
                                devc_contone_gray[position] = *psrc_temp;
                                position -= LAND_BITS;
                                xr--;
                            }           /* at loop exit xn will be <= xr */
                            psrc_temp++;
                        }
                    } else {
                        position = penum->ht_landscape.curr_pos;
                        /* Code up special cases for when we have no scaling
                           and 2x scaling which we will run into in 300 and
                           600dpi devices and content */
                        if (src_size == dest_height) {
                            for (k = 0; k < data_length; k++) {
                                devc_contone_gray[position] = psrc_cm[k];
                                position += LAND_BITS;
                            }
                        } else if (src_size*2 == dest_height) {
                            for (k = 0; k < data_length; k+=2) {
                                offset = fixed2int_var_rounded(fixed_half * k);
                                devc_contone_gray[position] =
                                    devc_contone_gray[position + LAND_BITS] =
                                    psrc_cm[offset];
                                position += 2*LAND_BITS;
                            }
                        } else {
                            /* use dda */
                            psrc_temp = psrc_cm;
                            for (k=0; k<src_size; k++) {
                                dda_next(dda_ht);
                                xn = fixed2int_var_rounded(dda_current(dda_ht));
                                while (xr < xn) {
                                    devc_contone_gray[position] = *psrc_temp;
                                    position += LAND_BITS;
                                    xr++;
                                }           /* at loop exit xn will be >= xr */
                                psrc_temp++;
                            }
                        }
                    }
                    /* Store the width information and update our counts */
                    penum->ht_landscape.count += vdi;
                    penum->ht_landscape.widths[penum->ht_landscape.curr_pos] = vdi;
                    penum->ht_landscape.curr_pos += penum->ht_landscape.index;
                    penum->ht_landscape.num_contones++;
                    break;
                default:
                    /* error not allowed */
                    break;
            }
        break;

        /* CMYK case */
        case 4:
            switch (posture) {
                /* CMYK portrait */
                case image_portrait:
                    if (penum->dst_width > 0) {
                        if (src_size == dest_width) {
                            memcpy(devc_contone[0], psrc_plane[0], data_length);
                            memcpy(devc_contone[1], psrc_plane[1], data_length);
                            memcpy(devc_contone[2], psrc_plane[2], data_length);
                            memcpy(devc_contone[3], psrc_plane[3], data_length);
                        } else if (src_size * 2 == dest_width) {
                            for (k = 0; k < data_length; k+=2) {
                                *(devc_contone[0]) = *(devc_contone[0]+1) =
                                    *psrc_plane[0]++;
                                *(devc_contone[1]) = *(devc_contone[1]+1) =
                                    *psrc_plane[1]++;
                                *(devc_contone[2]) = *(devc_contone[2]+1) =
                                    *psrc_plane[2]++;
                                *(devc_contone[3]) = *(devc_contone[3]+1) =
                                    *psrc_plane[3]++;
                                devc_contone[0] += 2;
                                devc_contone[1] += 2;
                                devc_contone[2] += 2;
                                devc_contone[3] += 2;
                            }
                        } else {
                        /* CMYK case, forward */
                            for (k=0, j=0; k<src_size; k++) {
                                dda_next(dda_ht);
                                xn = fixed2int_var_rounded(dda_current(dda_ht));
                                while (xr < xn) {
                                    *(devc_contone[0])++ = (psrc_plane[0])[j];
                                    *(devc_contone[1])++ = (psrc_plane[1])[j];
                                    *(devc_contone[2])++ = (psrc_plane[2])[j];
                                    *(devc_contone[3])++ = (psrc_plane[3])[j];
                                    xr++;
                                }           /* at loop exit xn will be >= xr */
                                j++;
                            }
                        }
                    } else {
                        /* CMYK case, backwards */
                        /* Move to the other end and we will decrement */
                        devc_contone[0] += (data_length - 1);
                        devc_contone[1] += (data_length - 1);
                        devc_contone[2] += (data_length - 1);
                        devc_contone[3] += (data_length - 1);
                        for (k=0, j=0; k<src_size; k++) {
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr > xn) {
                                *(devc_contone[0])-- = (psrc_plane[0])[j];
                                *(devc_contone[1])-- = (psrc_plane[1])[j];
                                *(devc_contone[2])-- = (psrc_plane[2])[j];
                                *(devc_contone[3])-- = (psrc_plane[3])[j];
                                xr--;
                            }           /* at loop exit xn will be <= xr */
                            j++;
                        }
                    }
                    break;
                /* CMYK landscape */
                case image_landscape:
                    /* Data is already color managed. */
                    /* We store the data at this point into columns in
                       seperate planes. Depending upon our landscape direction
                       we may be going left to right or right to left. */
                    if (penum->ht_landscape.flipy) {
                        position = penum->ht_landscape.curr_pos +
                                    LAND_BITS * (data_length - 1);
                        /* use dda */
                        for (k=0, i=0; k<src_size; k++) {
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr > xn) {
                                for (j = 0; j < spp_out; j++) {
                                    *(devc_contone[j] + position) = (psrc_plane[j])[i];
                                    position -= LAND_BITS;
                                }
                                xr--;
                            }           /* at loop exit xn will be <= xr */
                            i++;
                        }
                    } else {
                        position = penum->ht_landscape.curr_pos;
                        /* Code up special cases for when we have no scaling
                           and 2x scaling which we will run into in 300 and
                           600dpi devices and content */
                        /* Apply initial offset */
                        for (k = 0; k < spp_out; k++) {
                            devc_contone[k] = devc_contone[k] + position;
                        }
                        if (src_size == dest_height) {
                            for (k = 0; k < data_length; k++) {
                                /* Is it better to unwind this?  We know it is 4 */
                                for (j = 0; j < spp_out; j++) {
                                    *(devc_contone[j]) = (psrc_plane[j])[k];
                                    devc_contone[j] += LAND_BITS;
                                }
                            }
                        } else if (src_size*2 == dest_height) {
                            for (k = 0; k < data_length; k+=2) {
                                offset = fixed2int_var_rounded(fixed_half * k);
                                /* Is it better to unwind this?  We know it is 4 */
                                for (j = 0; j < spp_out; j++) {
                                    *(devc_contone[j]) =
                                      *(devc_contone[j] + LAND_BITS) =
                                      (psrc_plane[j])[offset];
                                    devc_contone[j] += 2 * LAND_BITS;
                                }
                            }
                        } else {
                            /* use dda */
                            for (k=0, i=0; k<src_size; k++) {
                                dda_next(dda_ht);
                                xn = fixed2int_var_rounded(dda_current(dda_ht));
                                while (xr > xn) {
                                    for (j = 0; j < spp_out; j++) {
                                        *(devc_contone[j] + position) = (psrc_plane[j])[i];
                                        position -= LAND_BITS;
                                    }
                                    xr--;
                                }           /* at loop exit xn will be <= xr */
                                i++;
                            }
                        }
                    }
                    /* Store the width information and update our counts */
                    penum->ht_landscape.count += vdi;
                    penum->ht_landscape.widths[penum->ht_landscape.curr_pos] = vdi;
                    penum->ht_landscape.curr_pos += penum->ht_landscape.index;
                    penum->ht_landscape.num_contones++;
                    break;
                default:
                    /* error not allowed */
                    break;
            }
        break;
        default:
            /* Not yet handled (e.g. CMY case) */
        break;
    }
    /* Apply threshold array to image data. It may be neccessary to invert
       depnding upon the polarity of the device */
flush:
    thresh_align = penum->thresh_buffer + offset_threshold;
    code = gxht_thresh_planes(penum, xrun, dest_width, dest_height,
                              thresh_align, dev, offset_contone,
                              contone_stride);
    /* Free cm buffer, if it was used */
    if (psrc_cm_start != NULL) {
        gs_free_object(penum->pgs->memory, (byte *)psrc_cm_start,
                       "image_render_color_thresh");
    }
    return code;
}