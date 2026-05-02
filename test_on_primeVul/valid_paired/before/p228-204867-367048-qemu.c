static void sm501_2d_operation(SM501State *s)
{
    int cmd = (s->twoD_control >> 16) & 0x1F;
    int rtl = s->twoD_control & BIT(27);
    int format = (s->twoD_stretch >> 20) & 0x3;
    int rop_mode = (s->twoD_control >> 15) & 0x1; /* 1 for rop2, else rop3 */
    /* 1 if rop2 source is the pattern, otherwise the source is the bitmap */
    int rop2_source_is_pattern = (s->twoD_control >> 14) & 0x1;
    int rop = s->twoD_control & 0xFF;
    int dst_x = (s->twoD_destination >> 16) & 0x01FFF;
    int dst_y = s->twoD_destination & 0xFFFF;
    int width = (s->twoD_dimension >> 16) & 0x1FFF;
    int height = s->twoD_dimension & 0xFFFF;
    uint32_t dst_base = s->twoD_destination_base & 0x03FFFFFF;
    uint8_t *dst = s->local_mem + dst_base;
    int dst_pitch = (s->twoD_pitch >> 16) & 0x1FFF;
    int crt = (s->dc_crt_control & SM501_DC_CRT_CONTROL_SEL) ? 1 : 0;
    int fb_len = get_width(s, crt) * get_height(s, crt) * get_bpp(s, crt);

    if ((s->twoD_stretch >> 16) & 0xF) {
        qemu_log_mask(LOG_UNIMP, "sm501: only XY addressing is supported.\n");
        return;
    }

    if (rop_mode == 0) {
        if (rop != 0xcc) {
            /* Anything other than plain copies are not supported */
            qemu_log_mask(LOG_UNIMP, "sm501: rop3 mode with rop %x is not "
                          "supported.\n", rop);
        }
    } else {
        if (rop2_source_is_pattern && rop != 0x5) {
            /* For pattern source, we support only inverse dest */
            qemu_log_mask(LOG_UNIMP, "sm501: rop2 source being the pattern and "
                          "rop %x is not supported.\n", rop);
        } else {
            if (rop != 0x5 && rop != 0xc) {
                /* Anything other than plain copies or inverse dest is not
                 * supported */
                qemu_log_mask(LOG_UNIMP, "sm501: rop mode %x is not "
                              "supported.\n", rop);
            }
        }
    }

    if (s->twoD_source_base & BIT(27) || s->twoD_destination_base & BIT(27)) {
        qemu_log_mask(LOG_UNIMP, "sm501: only local memory is supported.\n");
        return;
    }

    switch (cmd) {
    case 0x00: /* copy area */
    {
        int src_x = (s->twoD_source >> 16) & 0x01FFF;
        int src_y = s->twoD_source & 0xFFFF;
        uint32_t src_base = s->twoD_source_base & 0x03FFFFFF;
        uint8_t *src = s->local_mem + src_base;
        int src_pitch = s->twoD_pitch & 0x1FFF;

#define COPY_AREA(_bpp, _pixel_type, rtl) {                                   \
        int y, x, index_d, index_s;                                           \
        for (y = 0; y < height; y++) {                              \
            for (x = 0; x < width; x++) {                           \
                _pixel_type val;                                              \
                                                                              \
                if (rtl) {                                                    \
                    index_s = ((src_y - y) * src_pitch + src_x - x) * _bpp;   \
                    index_d = ((dst_y - y) * dst_pitch + dst_x - x) * _bpp;   \
                } else {                                                      \
                    index_s = ((src_y + y) * src_pitch + src_x + x) * _bpp;   \
                    index_d = ((dst_y + y) * dst_pitch + dst_x + x) * _bpp;   \
                }                                                             \
                if (rop_mode == 1 && rop == 5) {                              \
                    /* Invert dest */                                         \
                    val = ~*(_pixel_type *)&dst[index_d];                     \
                } else {                                                      \
                    val = *(_pixel_type *)&src[index_s];                      \
                }                                                             \
                *(_pixel_type *)&dst[index_d] = val;                          \
            }                                                                 \
        }                                                                     \
    }
        switch (format) {
        case 0:
            COPY_AREA(1, uint8_t, rtl);
            break;
        case 1:
            COPY_AREA(2, uint16_t, rtl);
            break;
        case 2:
            COPY_AREA(4, uint32_t, rtl);
            break;
        }
        break;
    }
    case 0x01: /* fill rectangle */
    {
        uint32_t color = s->twoD_foreground;

#define FILL_RECT(_bpp, _pixel_type) {                                      \
        int y, x;                                                           \
        for (y = 0; y < height; y++) {                            \
            for (x = 0; x < width; x++) {                         \
                int index = ((dst_y + y) * dst_pitch + dst_x + x) * _bpp;   \
                *(_pixel_type *)&dst[index] = (_pixel_type)color;           \
            }                                                               \
        }                                                                   \
    }

        switch (format) {
        case 0:
            FILL_RECT(1, uint8_t);
            break;
        case 1:
            color = cpu_to_le16(color);
            FILL_RECT(2, uint16_t);
            break;
        case 2:
            color = cpu_to_le32(color);
            FILL_RECT(4, uint32_t);
            break;
        }
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