Status DecodeImageAPNG(Span<const uint8_t> bytes, ThreadPool* pool,
                       CodecInOut* io) {
  Reader r;
  unsigned int id, i, j, w, h, w0, h0, x0, y0;
  unsigned int delay_num, delay_den, dop, bop, rowbytes, imagesize;
  unsigned char sig[8];
  png_structp png_ptr;
  png_infop info_ptr;
  CHUNK chunk;
  CHUNK chunkIHDR;
  std::vector<CHUNK> chunksInfo;
  bool isAnimated = false;
  bool skipFirst = false;
  bool hasInfo = false;
  bool all_dispose_bg = true;
  APNGFrame frameRaw = {};

  r = {bytes.data(), bytes.data() + bytes.size()};
  // Not an aPNG => not an error
  unsigned char png_signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  if (r.Read(sig, 8) || memcmp(sig, png_signature, 8) != 0) {
    return false;
  }
  id = read_chunk(&r, &chunkIHDR);

  io->frames.clear();
  io->dec_pixels = 0;
  io->metadata.m.SetUintSamples(8);
  io->metadata.m.SetAlphaBits(8);
  io->metadata.m.color_encoding =
      ColorEncoding::SRGB();  // todo: get data from png metadata
  (void)io->dec_hints.Foreach(
      [](const std::string& key, const std::string& /*value*/) {
        JXL_WARNING("APNG decoder ignoring %s hint", key.c_str());
        return true;
      });

  bool errorstate = true;
  if (id == kId_IHDR && chunkIHDR.size == 25) {
    w0 = w = png_get_uint_32(chunkIHDR.p + 8);
    h0 = h = png_get_uint_32(chunkIHDR.p + 12);

    if (w > cMaxPNGSize || h > cMaxPNGSize) {
      return false;
    }

    x0 = 0;
    y0 = 0;
    delay_num = 1;
    delay_den = 10;
    dop = 0;
    bop = 0;
    rowbytes = w * 4;
    imagesize = h * rowbytes;

    frameRaw.p = new unsigned char[imagesize];
    frameRaw.rows = new png_bytep[h * sizeof(png_bytep)];
    for (j = 0; j < h; j++) frameRaw.rows[j] = frameRaw.p + j * rowbytes;

    if (!processing_start(png_ptr, info_ptr, (void*)&frameRaw, hasInfo,
                          chunkIHDR, chunksInfo)) {
      bool last_base_was_none = true;
      while (!r.Eof()) {
        id = read_chunk(&r, &chunk);
        if (!id) break;
        JXL_ASSERT(chunk.p != nullptr);

        if (id == kId_acTL && !hasInfo && !isAnimated) {
          isAnimated = true;
          skipFirst = true;
          io->metadata.m.have_animation = true;
          io->metadata.m.animation.tps_numerator = 1000;
        } else if (id == kId_IEND ||
                   (id == kId_fcTL && (!hasInfo || isAnimated))) {
          if (hasInfo) {
            if (!processing_finish(png_ptr, info_ptr)) {
              ImageBundle bundle(&io->metadata.m);
              bundle.duration = delay_num * 1000 / delay_den;
              bundle.origin.x0 = x0;
              bundle.origin.y0 = y0;
              // TODO(veluca): this could in principle be implemented.
              if (last_base_was_none && !all_dispose_bg &&
                  (x0 != 0 || y0 != 0 || w0 != w || h0 != h || bop != 0)) {
                return JXL_FAILURE(
                    "APNG with dispose-to-0 is not supported for non-full or "
                    "blended frames");
              }
              switch (dop) {
                case 0:
                  bundle.use_for_next_frame = true;
                  last_base_was_none = false;
                  all_dispose_bg = false;
                  break;
                case 2:
                  bundle.use_for_next_frame = false;
                  all_dispose_bg = false;
                  break;
                default:
                  bundle.use_for_next_frame = false;
                  last_base_was_none = true;
              }
              bundle.blend = bop != 0;
              io->dec_pixels += w0 * h0;

              Image3F sub_frame(w0, h0);
              ImageF sub_frame_alpha(w0, h0);
              for (size_t y = 0; y < h0; ++y) {
                float* const JXL_RESTRICT row_r = sub_frame.PlaneRow(0, y);
                float* const JXL_RESTRICT row_g = sub_frame.PlaneRow(1, y);
                float* const JXL_RESTRICT row_b = sub_frame.PlaneRow(2, y);
                float* const JXL_RESTRICT row_alpha = sub_frame_alpha.Row(y);
                uint8_t* const f = frameRaw.rows[y];
                for (size_t x = 0; x < w0; ++x) {
                  if (f[4 * x + 3] == 0) {
                    row_alpha[x] = 0;
                    row_r[x] = 0;
                    row_g[x] = 0;
                    row_b[x] = 0;
                    continue;
                  }
                  row_r[x] = f[4 * x + 0] * (1.f / 255);
                  row_g[x] = f[4 * x + 1] * (1.f / 255);
                  row_b[x] = f[4 * x + 2] * (1.f / 255);
                  row_alpha[x] = f[4 * x + 3] * (1.f / 255);
                }
              }
              bundle.SetFromImage(std::move(sub_frame), ColorEncoding::SRGB());
              bundle.SetAlpha(std::move(sub_frame_alpha),
                              /*alpha_is_premultiplied=*/false);
              io->frames.push_back(std::move(bundle));
            } else {
              delete[] chunk.p;
              break;
            }
          }

          if (id == kId_IEND) {
            errorstate = false;
            break;
          }
          // At this point the old frame is done. Let's start a new one.
          w0 = png_get_uint_32(chunk.p + 12);
          h0 = png_get_uint_32(chunk.p + 16);
          x0 = png_get_uint_32(chunk.p + 20);
          y0 = png_get_uint_32(chunk.p + 24);
          delay_num = png_get_uint_16(chunk.p + 28);
          delay_den = png_get_uint_16(chunk.p + 30);
          dop = chunk.p[32];
          bop = chunk.p[33];

          if (w0 > cMaxPNGSize || h0 > cMaxPNGSize || x0 > cMaxPNGSize ||
              y0 > cMaxPNGSize || x0 + w0 > w || y0 + h0 > h || dop > 2 ||
              bop > 1) {
            delete[] chunk.p;
            break;
          }

          if (hasInfo) {
            memcpy(chunkIHDR.p + 8, chunk.p + 12, 8);
            if (processing_start(png_ptr, info_ptr, (void*)&frameRaw, hasInfo,
                                 chunkIHDR, chunksInfo)) {
              delete[] chunk.p;
              break;
            }
          } else
            skipFirst = false;

          if (io->frames.size() == (skipFirst ? 1 : 0)) {
            bop = 0;
            if (dop == 2) dop = 1;
          }
        } else if (id == kId_IDAT) {
          hasInfo = true;
          if (processing_data(png_ptr, info_ptr, chunk.p, chunk.size)) {
            delete[] chunk.p;
            break;
          }
        } else if (id == kId_fdAT && isAnimated) {
          png_save_uint_32(chunk.p + 4, chunk.size - 16);
          memcpy(chunk.p + 8, "IDAT", 4);
          if (processing_data(png_ptr, info_ptr, chunk.p + 4, chunk.size - 4)) {
            delete[] chunk.p;
            break;
          }
        } else if (!isAbc(chunk.p[4]) || !isAbc(chunk.p[5]) ||
                   !isAbc(chunk.p[6]) || !isAbc(chunk.p[7])) {
          delete[] chunk.p;
          break;
        } else if (!hasInfo) {
          if (processing_data(png_ptr, info_ptr, chunk.p, chunk.size)) {
            delete[] chunk.p;
            break;
          }
          chunksInfo.push_back(chunk);
          continue;
        }
        delete[] chunk.p;
      }
    }
    delete[] frameRaw.rows;
    delete[] frameRaw.p;
  }

  for (i = 0; i < chunksInfo.size(); i++) delete[] chunksInfo[i].p;

  chunksInfo.clear();
  delete[] chunkIHDR.p;

  if (errorstate) return false;
  SetIntensityTarget(io);
  return true;
}