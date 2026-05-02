tiff12_print_page(gx_device_printer * pdev, gp_file * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code;

    /* open the TIFF device */
    if (gdev_prn_file_is_new(pdev)) {
        tfdev->tif = tiff_from_filep(pdev, pdev->dname, file, tfdev->BigEndian, tfdev->UseBigTIFF);
        if (!tfdev->tif)
            return_error(gs_error_invalidfileaccess);
    }

    code = gdev_tiff_begin_page(tfdev, file);
    if (code < 0)
        return code;

    TIFFSetField(tfdev->tif, TIFFTAG_BITSPERSAMPLE, 4);
    tiff_set_rgb_fields(tfdev);

    TIFFCheckpointDirectory(tfdev->tif);

    /* Write the page data. */
    {
        int y;
        int size = gdev_prn_raster(pdev);
        byte *data = gs_alloc_bytes(pdev->memory, size, "tiff12_print_page");

        if (data == 0)
            return_error(gs_error_VMerror);

        memset(data, 0, size);

        for (y = 0; y < pdev->height; ++y) {
            const byte *src;
            byte *dest;
            int x;

            code = gdev_prn_copy_scan_lines(pdev, y, data, size);
            if (code < 0)
                break;

            for (src = data, dest = data, x = 0; x < size;
                 src += 6, dest += 3, x += 6
                ) {
                dest[0] = (src[0] & 0xf0) | (src[1] >> 4);
                dest[1] = (src[2] & 0xf0) | (src[3] >> 4);
                dest[2] = (src[4] & 0xf0) | (src[5] >> 4);
            }
            TIFFWriteScanline(tfdev->tif, data, y, 0);
        }
        gs_free_object(pdev->memory, data, "tiff12_print_page");

        TIFFWriteDirectory(tfdev->tif);
    }

    return code;
}