static int DecodeChunk(EXRImage *exr_image, const EXRHeader *exr_header,
                       const std::vector<tinyexr::tinyexr_uint64> &offsets,
                       const unsigned char *head, const size_t size,
                       std::string *err) {
  int num_channels = exr_header->num_channels;

  int num_scanline_blocks = 1;
  if (exr_header->compression_type == TINYEXR_COMPRESSIONTYPE_ZIP) {
    num_scanline_blocks = 16;
  } else if (exr_header->compression_type == TINYEXR_COMPRESSIONTYPE_PIZ) {
    num_scanline_blocks = 32;
  } else if (exr_header->compression_type == TINYEXR_COMPRESSIONTYPE_ZFP) {
    num_scanline_blocks = 16;
  }

  int data_width = exr_header->data_window[2] - exr_header->data_window[0] + 1;
  int data_height = exr_header->data_window[3] - exr_header->data_window[1] + 1;

  if ((data_width < 0) || (data_height < 0)) {
    if (err) {
      std::stringstream ss;
      ss << "Invalid data width or data height: " << data_width << ", "
         << data_height << std::endl;
      (*err) += ss.str();
    }
    return TINYEXR_ERROR_INVALID_DATA;
  }

  // Do not allow too large data_width and data_height. header invalid?
  {
    const int threshold = 1024 * 8192;  // heuristics
    if ((data_width > threshold) || (data_height > threshold)) {
      if (err) {
        std::stringstream ss;
        ss << "data_with or data_height too large. data_width: " << data_width
           << ", "
           << "data_height = " << data_height << std::endl;
        (*err) += ss.str();
      }
      return TINYEXR_ERROR_INVALID_DATA;
    }
  }

  size_t num_blocks = offsets.size();

  std::vector<size_t> channel_offset_list;
  int pixel_data_size = 0;
  size_t channel_offset = 0;
  if (!tinyexr::ComputeChannelLayout(&channel_offset_list, &pixel_data_size,
                                     &channel_offset, num_channels,
                                     exr_header->channels)) {
    if (err) {
      (*err) += "Failed to compute channel layout.\n";
    }
    return TINYEXR_ERROR_INVALID_DATA;
  }

  bool invalid_data = false;  // TODO(LTE): Use atomic lock for MT safety.

  if (exr_header->tiled) {
    // value check
    if (exr_header->tile_size_x < 0) {
      if (err) {
        std::stringstream ss;
        ss << "Invalid tile size x : " << exr_header->tile_size_x << "\n";
        (*err) += ss.str();
      }
      return TINYEXR_ERROR_INVALID_HEADER;
    }

    if (exr_header->tile_size_y < 0) {
      if (err) {
        std::stringstream ss;
        ss << "Invalid tile size y : " << exr_header->tile_size_y << "\n";
        (*err) += ss.str();
      }
      return TINYEXR_ERROR_INVALID_HEADER;
    }

    size_t num_tiles = offsets.size();  // = # of blocks

    exr_image->tiles = static_cast<EXRTile *>(
        calloc(sizeof(EXRTile), static_cast<size_t>(num_tiles)));

    for (size_t tile_idx = 0; tile_idx < num_tiles; tile_idx++) {
      // Allocate memory for each tile.
      exr_image->tiles[tile_idx].images = tinyexr::AllocateImage(
          num_channels, exr_header->channels, exr_header->requested_pixel_types,
          exr_header->tile_size_x, exr_header->tile_size_y);

      // 16 byte: tile coordinates
      // 4 byte : data size
      // ~      : data(uncompressed or compressed)
      if (offsets[tile_idx] + sizeof(int) * 5 > size) {
        if (err) {
          (*err) += "Insufficient data size.\n";
        }
        return TINYEXR_ERROR_INVALID_DATA;
      }

      size_t data_size = size_t(size - (offsets[tile_idx] + sizeof(int) * 5));
      const unsigned char *data_ptr =
          reinterpret_cast<const unsigned char *>(head + offsets[tile_idx]);

      int tile_coordinates[4];
      memcpy(tile_coordinates, data_ptr, sizeof(int) * 4);
      tinyexr::swap4(reinterpret_cast<unsigned int *>(&tile_coordinates[0]));
      tinyexr::swap4(reinterpret_cast<unsigned int *>(&tile_coordinates[1]));
      tinyexr::swap4(reinterpret_cast<unsigned int *>(&tile_coordinates[2]));
      tinyexr::swap4(reinterpret_cast<unsigned int *>(&tile_coordinates[3]));

      // @todo{ LoD }
      if (tile_coordinates[2] != 0) {
        return TINYEXR_ERROR_UNSUPPORTED_FEATURE;
      }
      if (tile_coordinates[3] != 0) {
        return TINYEXR_ERROR_UNSUPPORTED_FEATURE;
      }

      int data_len;
      memcpy(&data_len, data_ptr + 16,
             sizeof(int));  // 16 = sizeof(tile_coordinates)
      tinyexr::swap4(reinterpret_cast<unsigned int *>(&data_len));

      if (data_len < 4 || size_t(data_len) > data_size) {
        if (err) {
          (*err) += "Insufficient data length.\n";
        }
        return TINYEXR_ERROR_INVALID_DATA;
      }

      // Move to data addr: 20 = 16 + 4;
      data_ptr += 20;

      tinyexr::DecodeTiledPixelData(
          exr_image->tiles[tile_idx].images,
          &(exr_image->tiles[tile_idx].width),
          &(exr_image->tiles[tile_idx].height),
          exr_header->requested_pixel_types, data_ptr,
          static_cast<size_t>(data_len), exr_header->compression_type,
          exr_header->line_order, data_width, data_height, tile_coordinates[0],
          tile_coordinates[1], exr_header->tile_size_x, exr_header->tile_size_y,
          static_cast<size_t>(pixel_data_size),
          static_cast<size_t>(exr_header->num_custom_attributes),
          exr_header->custom_attributes,
          static_cast<size_t>(exr_header->num_channels), exr_header->channels,
          channel_offset_list);

      exr_image->tiles[tile_idx].offset_x = tile_coordinates[0];
      exr_image->tiles[tile_idx].offset_y = tile_coordinates[1];
      exr_image->tiles[tile_idx].level_x = tile_coordinates[2];
      exr_image->tiles[tile_idx].level_y = tile_coordinates[3];

      exr_image->num_tiles = static_cast<int>(num_tiles);
    }
  } else {  // scanline format

    // Don't allow too large image(256GB * pixel_data_size or more). Workaround
    // for #104.
    size_t total_data_len =
        size_t(data_width) * size_t(data_height) * size_t(num_channels);
    const bool total_data_len_overflown = sizeof(void*) == 8 ? (total_data_len >= 0x4000000000) : false;
    if ((total_data_len == 0) || total_data_len_overflown ) {
      if (err) {
        std::stringstream ss;
        ss << "Image data size is zero or too large: width = " << data_width
           << ", height = " << data_height << ", channels = " << num_channels
           << std::endl;
        (*err) += ss.str();
      }
      return TINYEXR_ERROR_INVALID_DATA;
    }

    exr_image->images = tinyexr::AllocateImage(
        num_channels, exr_header->channels, exr_header->requested_pixel_types,
        data_width, data_height);

#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (int y = 0; y < static_cast<int>(num_blocks); y++) {
      size_t y_idx = static_cast<size_t>(y);

      if (offsets[y_idx] + sizeof(int) * 2 > size) {
        invalid_data = true;
      } else {
        // 4 byte: scan line
        // 4 byte: data size
        // ~     : pixel data(uncompressed or compressed)
        size_t data_size = size_t(size - (offsets[y_idx] + sizeof(int) * 2));
        const unsigned char *data_ptr =
            reinterpret_cast<const unsigned char *>(head + offsets[y_idx]);

        int line_no;
        memcpy(&line_no, data_ptr, sizeof(int));
        int data_len;
        memcpy(&data_len, data_ptr + 4, sizeof(int));
        tinyexr::swap4(reinterpret_cast<unsigned int *>(&line_no));
        tinyexr::swap4(reinterpret_cast<unsigned int *>(&data_len));

        if (size_t(data_len) > data_size) {
          invalid_data = true;
        } else if (data_len == 0) {
          // TODO(syoyo): May be ok to raise the threshold for example `data_len
          // < 4`
          invalid_data = true;
        } else {
          // line_no may be negative.
          int end_line_no = (std::min)(line_no + num_scanline_blocks,
                                       (exr_header->data_window[3] + 1));

          int num_lines = end_line_no - line_no;

          if (num_lines <= 0) {
            invalid_data = true;
          } else {
            // Move to data addr: 8 = 4 + 4;
            data_ptr += 8;

            // Adjust line_no with data_window.bmin.y

            // overflow check
            tinyexr_int64 lno = static_cast<tinyexr_int64>(line_no) - static_cast<tinyexr_int64>(exr_header->data_window[1]);
            if (lno > std::numeric_limits<int>::max()) {
              line_no = -1; // invalid
            } else if (lno < -std::numeric_limits<int>::max()) {
              line_no = -1; // invalid
            } else {
              line_no -= exr_header->data_window[1];
            }

            if (line_no < 0) {
              invalid_data = true;
            } else {
              if (!tinyexr::DecodePixelData(
                      exr_image->images, exr_header->requested_pixel_types,
                      data_ptr, static_cast<size_t>(data_len),
                      exr_header->compression_type, exr_header->line_order,
                      data_width, data_height, data_width, y, line_no,
                      num_lines, static_cast<size_t>(pixel_data_size),
                      static_cast<size_t>(exr_header->num_custom_attributes),
                      exr_header->custom_attributes,
                      static_cast<size_t>(exr_header->num_channels),
                      exr_header->channels, channel_offset_list)) {
                invalid_data = true;
              }
            }
          }
        }
      }
    }  // omp parallel
  }

  if (invalid_data) {
    if (err) {
      std::stringstream ss;
      (*err) += "Invalid data found when decoding pixels.\n";
    }
    return TINYEXR_ERROR_INVALID_DATA;
  }

  // Overwrite `pixel_type` with `requested_pixel_type`.
  {
    for (int c = 0; c < exr_header->num_channels; c++) {
      exr_header->pixel_types[c] = exr_header->requested_pixel_types[c];
    }
  }

  {
    exr_image->num_channels = num_channels;

    exr_image->width = data_width;
    exr_image->height = data_height;
  }

  return TINYEXR_SUCCESS;
}