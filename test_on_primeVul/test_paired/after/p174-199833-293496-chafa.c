gif_internal_decode_frame(gif_animation *gif,
                          unsigned int frame,
                          bool clear_image)
{
        unsigned int index = 0;
        const unsigned char *gif_data, *gif_end;
        ssize_t gif_bytes;
        unsigned int width, height, offset_x, offset_y;
        unsigned int flags, colour_table_size, interlace;
        unsigned int *colour_table;
        unsigned int *frame_data = 0;	// Set to 0 for no warnings
        unsigned int *frame_scanline;
        ssize_t save_buffer_position;
        unsigned int return_value = 0;
        unsigned int x, y, decode_y, burst_bytes;
        register unsigned char colour;

        /* If the GIF has no frame data, frame holders will not be allocated in
         * gif_initialise() */
        if (gif->frames == NULL) {
                return GIF_INSUFFICIENT_DATA;
        }

        /* Ensure this frame is supposed to be decoded */
        if (gif->frames[frame].display == false) {
                return GIF_OK;
        }

        /* Ensure the frame is in range to decode */
        if (frame > gif->frame_count_partial) {
                return GIF_INSUFFICIENT_DATA;
        }

        /* done if frame is already decoded */
        if ((!clear_image) &&
            ((int)frame == gif->decoded_frame)) {
                return GIF_OK;
        }

        /* Get the start of our frame data and the end of the GIF data */
        gif_data = gif->gif_data + gif->frames[frame].frame_pointer;
        gif_end = gif->gif_data + gif->buffer_size;
        gif_bytes = (gif_end - gif_data);

        /*
         * Ensure there is a minimal amount of data to proceed.  The shortest
         * block of data is a 10-byte image descriptor + 1-byte gif trailer
         */
        if (gif_bytes < 12) {
                return GIF_INSUFFICIENT_FRAME_DATA;
        }

        /* Save the buffer position */
        save_buffer_position = gif->buffer_position;
        gif->buffer_position = gif_data - gif->gif_data;

        /* Skip any extensions because they have allready been processed */
        if ((return_value = gif_skip_frame_extensions(gif)) != GIF_OK) {
                goto gif_decode_frame_exit;
        }
        gif_data = (gif->gif_data + gif->buffer_position);
        gif_bytes = (gif_end - gif_data);

        /* Ensure we have enough data for the 10-byte image descriptor + 1-byte
         * gif trailer
         */
        if (gif_bytes < 12) {
                return_value = GIF_INSUFFICIENT_FRAME_DATA;
                goto gif_decode_frame_exit;
        }

        /* 10-byte Image Descriptor is:
         *
         *	+0	CHAR	Image Separator (0x2c)
         *	+1	SHORT	Image Left Position
         *	+3	SHORT	Image Top Position
         *	+5	SHORT	Width
         *	+7	SHORT	Height
         *	+9	CHAR	__Packed Fields__
         *			1BIT	Local Colour Table Flag
         *			1BIT	Interlace Flag
         *			1BIT	Sort Flag
         *			2BITS	Reserved
         *			3BITS	Size of Local Colour Table
         */
        if (gif_data[0] != GIF_IMAGE_SEPARATOR) {
                return_value = GIF_DATA_ERROR;
                goto gif_decode_frame_exit;
        }
        offset_x = gif_data[1] | (gif_data[2] << 8);
        offset_y = gif_data[3] | (gif_data[4] << 8);
        width = gif_data[5] | (gif_data[6] << 8);
        height = gif_data[7] | (gif_data[8] << 8);

        /* Boundary checking - shouldn't ever happen except unless the data has
         * been modified since initialisation.
         */
        if ((offset_x + width > gif->width) ||
            (offset_y + height > gif->height)) {
                return_value = GIF_DATA_ERROR;
                goto gif_decode_frame_exit;
        }

        /* Decode the flags */
        flags = gif_data[9];
        colour_table_size = 2 << (flags & GIF_COLOUR_TABLE_SIZE_MASK);
        interlace = flags & GIF_INTERLACE_MASK;

        /* Advance data pointer to next block either colour table or image
         * data.
         */
        gif_data += 10;
        gif_bytes = (gif_end - gif_data);

        /* Set up the colour table */
        if (flags & GIF_COLOUR_TABLE_MASK) {
                if (gif_bytes < (int)(3 * colour_table_size)) {
                        return_value = GIF_INSUFFICIENT_FRAME_DATA;
                        goto gif_decode_frame_exit;
                }
                colour_table = gif->local_colour_table;
                if (!clear_image) {
                        for (index = 0; index < colour_table_size; index++) {
                                /* Gif colour map contents are r,g,b.
                                 *
                                 * We want to pack them bytewise into the
                                 * colour table, such that the red component
                                 * is in byte 0 and the alpha component is in
                                 * byte 3.
                                 */
                                unsigned char *entry =
                                        (unsigned char *) &colour_table[index];

                                entry[0] = gif_data[0];	/* r */
                                entry[1] = gif_data[1];	/* g */
                                entry[2] = gif_data[2];	/* b */
                                entry[3] = 0xff;	/* a */

                                gif_data += 3;
                        }
                } else {
                        gif_data += 3 * colour_table_size;
                }
                gif_bytes = (gif_end - gif_data);
        } else {
                colour_table = gif->global_colour_table;
        }

        /* Ensure sufficient data remains */
        if (gif_bytes < 1) {
                return_value = GIF_INSUFFICIENT_FRAME_DATA;
                goto gif_decode_frame_exit;
        }

        /* check for an end marker */
        if (gif_data[0] == GIF_TRAILER) {
                return_value = GIF_OK;
                goto gif_decode_frame_exit;
        }

        /* Get the frame data */
        assert(gif->bitmap_callbacks.bitmap_get_buffer);
        frame_data = (void *)gif->bitmap_callbacks.bitmap_get_buffer(gif->frame_image);
        if (!frame_data) {
                return GIF_INSUFFICIENT_MEMORY;
        }

        /* If we are clearing the image we just clear, if not decode */
        if (!clear_image) {
                lzw_result res;
                const uint8_t *stack_base;
                const uint8_t *stack_pos;

                /* Ensure we have enough data for a 1-byte LZW code size +
                 * 1-byte gif trailer
                 */
                if (gif_bytes < 2) {
                        return_value = GIF_INSUFFICIENT_FRAME_DATA;
                        goto gif_decode_frame_exit;
                }

                /* If we only have a 1-byte LZW code size + 1-byte gif trailer,
                 * we're finished
                 */
                if ((gif_bytes == 2) && (gif_data[1] == GIF_TRAILER)) {
                        return_value = GIF_OK;
                        goto gif_decode_frame_exit;
                }

                /* If the previous frame's disposal method requires we restore
                 * the background colour or this is the first frame, clear
                 * the frame data
                 */
                if ((frame == 0) || (gif->decoded_frame == GIF_INVALID_FRAME)) {
                        memset((char*)frame_data,
                               GIF_TRANSPARENT_COLOUR,
                               gif->width * gif->height * sizeof(int));
                        gif->decoded_frame = frame;
                        /* The line below would fill the image with its
                         * background color, but because GIFs support
                         * transparency we likely wouldn't want to do that. */
                        /* memset((char*)frame_data, colour_table[gif->background_index], gif->width * gif->height * sizeof(int)); */
                } else if ((frame != 0) &&
                           (gif->frames[frame - 1].disposal_method == GIF_FRAME_CLEAR)) {
                        return_value = gif_internal_decode_frame(gif,
                                                                 (frame - 1),
                                                                 true);
                        if (return_value != GIF_OK) {
                                goto gif_decode_frame_exit;
                        }

                } else if ((frame != 0) &&
                           (gif->frames[frame - 1].disposal_method == GIF_FRAME_RESTORE)) {
                        /*
                         * If the previous frame's disposal method requires we
                         * restore the previous image, find the last image set
                         * to "do not dispose" and get that frame data
                         */
                        int last_undisposed_frame = frame - 2;
                        while ((last_undisposed_frame >= 0) &&
                               (gif->frames[last_undisposed_frame].disposal_method == GIF_FRAME_RESTORE)) {
                                last_undisposed_frame--;
                        }

                        /* If we don't find one, clear the frame data */
                        if (last_undisposed_frame == -1) {
                                /* see notes above on transparency
                                 * vs. background color
                                 */
                                memset((char*)frame_data,
                                       GIF_TRANSPARENT_COLOUR,
                                       gif->width * gif->height * sizeof(int));
                        } else {
                                return_value = gif_internal_decode_frame(gif, last_undisposed_frame, false);
                                if (return_value != GIF_OK) {
                                        goto gif_decode_frame_exit;
                                }
                                /* Get this frame's data */
                                assert(gif->bitmap_callbacks.bitmap_get_buffer);
                                frame_data = (void *)gif->bitmap_callbacks.bitmap_get_buffer(gif->frame_image);
                                if (!frame_data) {
                                        return GIF_INSUFFICIENT_MEMORY;
                                }
                        }
                }
                gif->decoded_frame = frame;
                gif->buffer_position = (gif_data - gif->gif_data) + 1;

                /* Initialise the LZW decoding */
                res = lzw_decode_init(gif->lzw_ctx, gif->gif_data,
                                gif->buffer_size, gif->buffer_position,
                                gif_data[0], &stack_base, &stack_pos);
                if (res != LZW_OK) {
                        return gif_error_from_lzw(res);
                }

                /* Decompress the data */
                for (y = 0; y < height; y++) {
                        if (interlace) {
                                decode_y = gif_interlaced_line(height, y) + offset_y;
                        } else {
                                decode_y = y + offset_y;
                        }
                        frame_scanline = frame_data + offset_x + (decode_y * gif->width);

                        /* Rather than decoding pixel by pixel, we try to burst
                         * out streams of data to remove the need for end-of
                         * data checks every pixel.
                         */
                        x = width;
                        while (x > 0) {
                                burst_bytes = (stack_pos - stack_base);
                                if (burst_bytes > 0) {
                                        if (burst_bytes > x) {
                                                burst_bytes = x;
                                        }
                                        x -= burst_bytes;
                                        while (burst_bytes-- > 0) {
                                                colour = *--stack_pos;
                                                if (((gif->frames[frame].transparency) &&
                                                     (colour != gif->frames[frame].transparency_index)) ||
                                                    (!gif->frames[frame].transparency)) {
                                                        *frame_scanline = colour_table[colour];
                                                }
                                                frame_scanline++;
                                        }
                                } else {
                                        res = lzw_decode(gif->lzw_ctx, &stack_pos);
                                        if (res != LZW_OK) {
                                                /* Unexpected end of frame, try to recover */
                                                if (res == LZW_OK_EOD) {
                                                        return_value = GIF_OK;
                                                } else {
                                                        return_value = gif_error_from_lzw(res);
                                                }
                                                goto gif_decode_frame_exit;
                                        }
                                }
                        }
                }
        } else {
                /* Clear our frame */
                if (gif->frames[frame].disposal_method == GIF_FRAME_CLEAR) {
                        for (y = 0; y < height; y++) {
                                frame_scanline = frame_data + offset_x + ((offset_y + y) * gif->width);
                                if (gif->frames[frame].transparency) {
                                        memset(frame_scanline,
                                               GIF_TRANSPARENT_COLOUR,
                                               width * 4);
                                } else {
                                        memset(frame_scanline,
                                               colour_table[gif->background_index],
                                               width * 4);
                                }
                        }
                }
        }
gif_decode_frame_exit:

        /* Check if we should test for optimisation */
        if (gif->frames[frame].virgin) {
                if (gif->bitmap_callbacks.bitmap_test_opaque) {
                        gif->frames[frame].opaque = gif->bitmap_callbacks.bitmap_test_opaque(gif->frame_image);
                } else {
                        gif->frames[frame].opaque = false;
                }
                gif->frames[frame].virgin = false;
        }

        if (gif->bitmap_callbacks.bitmap_set_opaque) {
                gif->bitmap_callbacks.bitmap_set_opaque(gif->frame_image, gif->frames[frame].opaque);
        }

        if (gif->bitmap_callbacks.bitmap_modified) {
                gif->bitmap_callbacks.bitmap_modified(gif->frame_image);
        }

        /* Restore the buffer position */
        gif->buffer_position = save_buffer_position;

        return return_value;
}