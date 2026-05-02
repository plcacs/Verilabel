static void huffman_decode_row(x3f_info_t *I, x3f_directory_entry_t *DE,
                               int bits, int row, int offset, int *minimum)
{
  x3f_directory_entry_header_t *DEH = &DE->header;
  x3f_image_data_t *ID = &DEH->data_subsection.image_data;
  x3f_huffman_t *HUF = ID->huffman;

  int16_t c[3] = {(int16_t)offset, (int16_t)offset, (int16_t)offset};
  int col;
  bit_state_t BS;

  set_bit_state(&BS, (uint8_t *)ID->data + HUF->row_offsets.element[row]);

  for (col = 0; col < ID->columns; col++)
  {
    int color;

    for (color = 0; color < 3; color++)
    {
      uint16_t c_fix;

      c[color] += get_huffman_diff(&BS, &HUF->tree);
      if (c[color] < 0)
      {
        c_fix = 0;
        if (c[color] < *minimum)
          *minimum = c[color];
      }
      else
      {
        c_fix = c[color];
      }

      switch (ID->type_format)
      {
      case X3F_IMAGE_RAW_HUFFMAN_X530:
      case X3F_IMAGE_RAW_HUFFMAN_10BIT:
        HUF->x3rgb16.data[3 * (row * ID->columns + col) + color] =
            (uint16_t)c_fix;
        break;
      case X3F_IMAGE_THUMB_HUFFMAN:
        HUF->rgb8.data[3 * (row * ID->columns + col) + color] = (uint8_t)c_fix;
        break;
      default:
        /* TODO: Shouldn't this be treated as a fatal error? */
        throw LIBRAW_EXCEPTION_IO_CORRUPT;
      }
    }
  }
}