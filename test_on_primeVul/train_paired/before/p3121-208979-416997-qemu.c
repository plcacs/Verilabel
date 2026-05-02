static void ati_cursor_define(ATIVGAState *s)
{
    uint8_t data[1024];
    uint8_t *src;
    int i, j, idx = 0;

    if ((s->regs.cur_offset & BIT(31)) || s->cursor_guest_mode) {
        return; /* Do not update cursor if locked or rendered by guest */
    }
    /* FIXME handle cur_hv_offs correctly */
    src = s->vga.vram_ptr + s->regs.cur_offset -
          (s->regs.cur_hv_offs >> 16) - (s->regs.cur_hv_offs & 0xffff) * 16;
    for (i = 0; i < 64; i++) {
        for (j = 0; j < 8; j++, idx++) {
            data[idx] = src[i * 16 + j];
            data[512 + idx] = src[i * 16 + j + 8];
        }
    }
    if (!s->cursor) {
        s->cursor = cursor_alloc(64, 64);
    }
    cursor_set_mono(s->cursor, s->regs.cur_color1, s->regs.cur_color0,
                    &data[512], 1, &data[0]);
    dpy_cursor_define(s->vga.con, s->cursor);
}