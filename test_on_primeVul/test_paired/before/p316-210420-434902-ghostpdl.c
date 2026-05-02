fill_threshhold_buffer(byte *dest_strip, byte *src_strip, int src_width,
                       int left_offset, int left_width, int num_tiles,
                       int right_width)
{
    byte *ptr_out_temp = dest_strip;
    int ii;

    /* Left part */
    memcpy(dest_strip, src_strip + left_offset, left_width);
    ptr_out_temp += left_width;
    /* Now the full parts */
    for (ii = 0; ii < num_tiles; ii++){
        memcpy(ptr_out_temp, src_strip, src_width);
        ptr_out_temp += src_width;
    }
    /* Now the remainder */
    memcpy(ptr_out_temp, src_strip, right_width);
#ifdef PACIFY_VALGRIND
    ptr_out_temp += right_width;
    ii = (dest_strip-ptr_out_temp) % (LAND_BITS-1);
    if (ii > 0)
        memset(ptr_out_temp, 0, ii);
#endif
}