lprn_is_black(gx_device_printer * pdev, int r, int h, int bx)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;

    int bh = lprn->nBh;
    int bpl = gdev_mem_bytes_per_scan_line(pdev);
    int x, y, y0;
    byte *p;
    int maxY = lprn->BlockLine / lprn->nBh * lprn->nBh;

    y0 = (r + h - bh) % maxY;
    for (y = 0; y < bh; y++) {
        p = &lprn->ImageBuf[(y0 + y) * bpl + bx * lprn->nBw];
        for (x = 0; x < lprn->nBw; x++)
            if (p[x] != 0)
                return 1;
    }
    return 0;
}