static void sm501_2d_operation(SM501State *s)
{
    int cmd = (s->twoD_control >> 16) & 0x1F;
    int rtl = s->twoD_control & BIT(27);
    int format = (s->twoD_stretch >> 20) & 0x3;
    int rop_mode = (s->twoD_control >> 15) & 0x1; /* 1 for rop2, else rop3 */
    /* 1 if rop2 source is the pattern, otherwise the source is the bitmap */
    int rop2_source_is_pattern = (s->twoD_control >> 14) & 0x1;
    int rop = s->twoD_control & 0xFF;
    unsigned int dst_x = (s->twoD_destination >> 16) & 0x01FFF;
    unsigned int dst_y = s->twoD_destination & 0xFFFF;
    unsigned int width = (s->twoD_dimension >> 16) & 0x1FFF;
    unsigned int height = s->twoD_dimension & 0xFFFF;
    uint32_t dst_base = s->twoD_destination_base & 0x03FFFFFF;
    unsigned int dst_pitch = (s->twoD_pitch >> 16) & 0x1FFF;
    int crt = (s->dc_crt_control & SM501_DC_CRT_CONTROL_SEL) ? 1 : 0;
    int fb_len = get_width(s, crt) * get_height(s, crt) * get_bpp(s, crt);

    if ((s->twoD_stretch >> 16) & 0xF) {
        qemu_log_mask(LOG_UNIMP, "sm501: only XY addressing is supported.\n");
        return;
    }

    if (s->twoD_source_base & BIT(27) || s->twoD_destination_base & BIT(27)) {
        qemu_log_mask(LOG_UNIMP, "sm501: only local memory is supported.\n");
        return;
    }

    if (!dst_pitch) {
        qemu_log_mask(LOG_GUEST_ERROR, "sm501: Zero dest pitch.\n");
        return;
    }

    if (!width || !height) {
        qemu_log_mask(LOG_GUEST_ERROR, "sm501: Zero size 2D op.\n");
        return;
    }

    if (rtl) {
        dst_x -= width - 1;
        dst_y -= height - 1;
    }

    if (dst_base >= get_local_mem_size(s) || dst_base +
        (dst_x + width + (dst_y + height) * (dst_pitch + width)) *
        (1 << format) >= get_local_mem_size(s)) {
        qemu_log_mask(LOG_GUEST_ERROR, "sm501: 2D op dest is outside vram.\n");
        return;
    }

    switch (cmd) {
    case 0: /* BitBlt */
    {
        unsigned int src_x = (s->twoD_source >> 16) & 0x01FFF;
        unsigned int src_y = s->twoD_source & 0xFFFF;
        uint32_t src_base = s->twoD_source_base & 0x03FFFFFF;
        unsigned int src_pitch = s->twoD_pitch & 0x1FFF;

        if (!src_pitch) {
            qemu_log_mask(LOG_GUEST_ERROR, "sm501: Zero src pitch.\n");
            return;
        }

        if (rtl) {
            src_x -= width - 1;
            src_y -= height - 1;
        }

        if (src_base >= get_local_mem_size(s) || src_base +
            (src_x + width + (src_y + height) * (src_pitch + width)) *
            (1 << format) >= get_local_mem_size(s)) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "sm501: 2D op src is outside vram.\n");
            return;
        }

        if ((rop_mode && rop == 0x5) || (!rop_mode && rop == 0x55)) {
            /* Invert dest, is there a way to do this with pixman? */
            unsigned int x, y, i;
            uint8_t *d = s->local_mem + dst_base;

            for (y = 0; y < height; y++) {
                i = (dst_x + (dst_y + y) * dst_pitch) * (1 << format);
                for (x = 0; x < width; x++, i += (1 << format)) {
                    switch (format) {
                    case 0:
                        d[i] = ~d[i];
                        break;
                    case 1:
                        *(uint16_t *)&d[i] = ~*(uint16_t *)&d[i];
                        break;
                    case 2:
                        *(uint32_t *)&d[i] = ~*(uint32_t *)&d[i];
                        break;
                    }
                }
            }
        } else {
            /* Do copy src for unimplemented ops, better than unpainted area */
            if ((rop_mode && (rop != 0xc || rop2_source_is_pattern)) ||
                (!rop_mode && rop != 0xcc)) {
                qemu_log_mask(LOG_UNIMP,
                              "sm501: rop%d op %x%s not implemented\n",
                              (rop_mode ? 2 : 3), rop,
                              (rop2_source_is_pattern ?
                                  " with pattern source" : ""));
            }
            /* Check for overlaps, this could be made more exact */
            uint32_t sb, se, db, de;
            sb = src_base + src_x + src_y * (width + src_pitch);
            se = sb + width + height * (width + src_pitch);
            db = dst_base + dst_x + dst_y * (width + dst_pitch);
            de = db + width + height * (width + dst_pitch);
            if (rtl && ((db >= sb && db <= se) || (de >= sb && de <= se))) {
                /* regions may overlap: copy via temporary */
                int llb = width * (1 << format);
                int tmp_stride = DIV_ROUND_UP(llb, sizeof(uint32_t));
                uint32_t *tmp = g_malloc(tmp_stride * sizeof(uint32_t) *
                                         height);
                pixman_blt((uint32_t *)&s->local_mem[src_base], tmp,
                           src_pitch * (1 << format) / sizeof(uint32_t),
                           tmp_stride, 8 * (1 << format), 8 * (1 << format),
                           src_x, src_y, 0, 0, width, height);
                pixman_blt(tmp, (uint32_t *)&s->local_mem[dst_base],
                           tmp_stride,
                           dst_pitch * (1 << format) / sizeof(uint32_t),
                           8 * (1 << format), 8 * (1 << format),
                           0, 0, dst_x, dst_y, width, height);
                g_free(tmp);
            } else {
                pixman_blt((uint32_t *)&s->local_mem[src_base],
                           (uint32_t *)&s->local_mem[dst_base],
                           src_pitch * (1 << format) / sizeof(uint32_t),
                           dst_pitch * (1 << format) / sizeof(uint32_t),
                           8 * (1 << format), 8 * (1 << format),
                           src_x, src_y, dst_x, dst_y, width, height);
            }
        }
        break;
    }
    case 1: /* Rectangle Fill */
    {
        uint32_t color = s->twoD_foreground;

        if (format == 2) {
            color = cpu_to_le32(color);
        } else if (format == 1) {
            color = cpu_to_le16(color);
        }

        pixman_fill((uint32_t *)&s->local_mem[dst_base],
                    dst_pitch * (1 << format) / sizeof(uint32_t),
                    8 * (1 << format), dst_x, dst_y, width, height, color);
        break;
    }
    default:
        qemu_log_mask(LOG_UNIMP, "sm501: not implemented 2D operation: %d\n",
                      cmd);
        return;
    }

    if (dst_base >= get_fb_addr(s, crt) &&
        dst_base <= get_fb_addr(s, crt) + fb_len) {
        int dst_len = MIN(fb_len, ((dst_y + height - 1) * dst_pitch +
                          dst_x + width) * (1 << format));
        if (dst_len) {
            memory_region_set_dirty(&s->local_mem_region, dst_base, dst_len);
        }
    }
}