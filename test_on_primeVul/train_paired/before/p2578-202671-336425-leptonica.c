findNextBorderPixel(l_int32    w,
                    l_int32    h,
                    l_uint32  *data,
                    l_int32    wpl,
                    l_int32    px,
                    l_int32    py,
                    l_int32   *pqpos,
                    l_int32   *pnpx,
                    l_int32   *pnpy)
{
l_int32    qpos, i, pos, npx, npy, val;
l_uint32  *line;

    qpos = *pqpos;
    for (i = 1; i < 8; i++) {
        pos = (qpos + i) % 8;
        npx = px + xpostab[pos];
        npy = py + ypostab[pos];
        line = data + npy * wpl;
        val = GET_DATA_BIT(line, npx);
        if (val) {
            *pnpx = npx;
            *pnpy = npy;
            *pqpos = qpostab[pos];
            return 0;
        }
    }

    return 1;
}