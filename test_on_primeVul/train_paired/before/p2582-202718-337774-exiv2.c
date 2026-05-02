    void WebPImage::doWriteMetadata(BasicIo& outIo)
    {
        if (!io_->isopen()) throw Error(kerInputDataReadFailed);
        if (!outIo.isopen()) throw Error(kerImageWriteFailed);

#ifdef EXIV2_DEBUG_MESSAGES
        std::cout << "Writing metadata" << std::endl;
#endif

        byte    data   [WEBP_TAG_SIZE*3];
        DataBuf chunkId(WEBP_TAG_SIZE+1);
        chunkId.pData_ [WEBP_TAG_SIZE] = '\0';

        io_->read(data, WEBP_TAG_SIZE * 3);
        uint64_t filesize = Exiv2::getULong(data + WEBP_TAG_SIZE, littleEndian);

        /* Set up header */
        if (outIo.write(data, WEBP_TAG_SIZE * 3) != WEBP_TAG_SIZE * 3)
            throw Error(kerImageWriteFailed);

        /* Parse Chunks */
        bool has_size  = false;
        bool has_xmp   = false;
        bool has_exif  = false;
        bool has_vp8x  = false;
        bool has_alpha = false;
        bool has_icc   = iccProfileDefined();

        int width      = 0;
        int height     = 0;

        byte       size_buff[WEBP_TAG_SIZE];
        Blob       blob;

        if (exifData_.count() > 0) {
            ExifParser::encode(blob, littleEndian, exifData_);
            if (blob.size() > 0) {
                has_exif = true;
            }
        }

        if (xmpData_.count() > 0 && !writeXmpFromPacket()) {
            XmpParser::encode(xmpPacket_, xmpData_,
                              XmpParser::useCompactFormat |
                              XmpParser::omitAllFormatting);
        }
        has_xmp = xmpPacket_.size() > 0;
        std::string xmp(xmpPacket_);

        /* Verify for a VP8X Chunk First before writing in
         case we have any exif or xmp data, also check
         for any chunks with alpha frame/layer set */
        while ( !io_->eof() && (uint64_t) io_->tell() < filesize) {
            io_->read(chunkId.pData_, WEBP_TAG_SIZE);
            io_->read(size_buff, WEBP_TAG_SIZE);
            long size = Exiv2::getULong(size_buff, littleEndian);
            DataBuf payload(size);
            io_->read(payload.pData_, payload.size_);
            byte c;
            if ( payload.size_ % 2 ) io_->read(&c,1);

            /* Chunk with information about features
             used in the file. */
            if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_VP8X) && !has_vp8x) {
                has_vp8x = true;
            }
            if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_VP8X) && !has_size) {
                has_size = true;
                byte size_buf[WEBP_TAG_SIZE];

                // Fetch width - stored in 24bits
                memcpy(&size_buf, &payload.pData_[4], 3);
                size_buf[3] = 0;
                width = Exiv2::getULong(size_buf, littleEndian) + 1;

                // Fetch height - stored in 24bits
                memcpy(&size_buf, &payload.pData_[7], 3);
                size_buf[3] = 0;
                height = Exiv2::getULong(size_buf, littleEndian) + 1;
            }

            /* Chunk with with animation control data. */
#ifdef __CHECK_FOR_ALPHA__  // Maybe in the future
            if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_ANIM) && !has_alpha) {
                has_alpha = true;
            }
#endif

            /* Chunk with with lossy image data. */
#ifdef __CHECK_FOR_ALPHA__ // Maybe in the future
            if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_VP8) && !has_alpha) {
                has_alpha = true;
            }
#endif
            if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_VP8) && !has_size) {
                has_size = true;
                byte size_buf[2];

                /* Refer to this https://tools.ietf.org/html/rfc6386
                   for height and width reference for VP8 chunks */

                // Fetch width - stored in 16bits
                memcpy(&size_buf, &payload.pData_[6], 2);
                width = Exiv2::getUShort(size_buf, littleEndian) & 0x3fff;

                // Fetch height - stored in 16bits
                memcpy(&size_buf, &payload.pData_[8], 2);
                height = Exiv2::getUShort(size_buf, littleEndian) & 0x3fff;
            }

            /* Chunk with with lossless image data. */
            if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_VP8L) && !has_alpha) {
                if ((payload.pData_[4] & WEBP_VP8X_ALPHA_BIT) == WEBP_VP8X_ALPHA_BIT) {
                    has_alpha = true;
                }
            }
            if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_VP8L) && !has_size) {
                has_size = true;
                byte size_buf_w[2];
                byte size_buf_h[3];

                /* For VP8L chunks width & height are stored in 28 bits
                   of a 32 bit field requires bitshifting to get actual
                   sizes. Width and height are split even into 14 bits
                   each. Refer to this https://goo.gl/bpgMJf */

                // Fetch width - 14 bits wide
                memcpy(&size_buf_w, &payload.pData_[1], 2);
                size_buf_w[1] &= 0x3F;
                width = Exiv2::getUShort(size_buf_w, littleEndian) + 1;

                // Fetch height - 14 bits wide
                memcpy(&size_buf_h, &payload.pData_[2], 3);
                size_buf_h[0] =
                  ((size_buf_h[0] >> 6) & 0x3) |
                    ((size_buf_h[1] & 0x3F) << 0x2);
                size_buf_h[1] =
                  ((size_buf_h[1] >> 6) & 0x3) |
                    ((size_buf_h[2] & 0xF) << 0x2);
                height = Exiv2::getUShort(size_buf_h, littleEndian) + 1;
            }

            /* Chunk with animation frame. */
            if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_ANMF) && !has_alpha) {
                if ((payload.pData_[5] & 0x2) == 0x2) {
                    has_alpha = true;
                }
            }
            if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_ANMF) && !has_size) {
                has_size = true;
                byte size_buf[WEBP_TAG_SIZE];

                // Fetch width - stored in 24bits
                memcpy(&size_buf, &payload.pData_[6], 3);
                size_buf[3] = 0;
                width = Exiv2::getULong(size_buf, littleEndian) + 1;

                // Fetch height - stored in 24bits
                memcpy(&size_buf, &payload.pData_[9], 3);
                size_buf[3] = 0;
                height = Exiv2::getULong(size_buf, littleEndian) + 1;
            }

            /* Chunk with alpha data. */
            if (equalsWebPTag(chunkId, "ALPH") && !has_alpha) {
                has_alpha = true;
            }
        }

        /* Inject a VP8X chunk if one isn't available. */
        if (!has_vp8x) {
            inject_VP8X(outIo, has_xmp, has_exif, has_alpha,
                        has_icc, width, height);
        }

        io_->seek(12, BasicIo::beg);
        while ( !io_->eof() && (uint64_t) io_->tell() < filesize) {
            io_->read(chunkId.pData_, 4);
            io_->read(size_buff, 4);

            long size = Exiv2::getULong(size_buff, littleEndian);

            DataBuf payload(size);
            io_->read(payload.pData_, size);
            if ( io_->tell() % 2 ) io_->seek(+1,BasicIo::cur); // skip pad

            if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_VP8X)) {
                if (has_icc){
                    payload.pData_[0] |= WEBP_VP8X_ICC_BIT;
                } else {
                    payload.pData_[0] &= ~WEBP_VP8X_ICC_BIT;
                }

                if (has_xmp){
                    payload.pData_[0] |= WEBP_VP8X_XMP_BIT;
                } else {
                    payload.pData_[0] &= ~WEBP_VP8X_XMP_BIT;
                }

                if (has_exif) {
                    payload.pData_[0] |= WEBP_VP8X_EXIF_BIT;
                } else {
                    payload.pData_[0] &= ~WEBP_VP8X_EXIF_BIT;
                }

                if (outIo.write(chunkId.pData_, WEBP_TAG_SIZE) != WEBP_TAG_SIZE)
                    throw Error(kerImageWriteFailed);
                if (outIo.write(size_buff, WEBP_TAG_SIZE) != WEBP_TAG_SIZE)
                    throw Error(kerImageWriteFailed);
                if (outIo.write(payload.pData_, payload.size_) != payload.size_)
                    throw Error(kerImageWriteFailed);
                if (outIo.tell() % 2) {
                    if (outIo.write(&WEBP_PAD_ODD, 1) != 1) throw Error(kerImageWriteFailed);
                }

                if (has_icc) {
                    if (outIo.write((const byte*)WEBP_CHUNK_HEADER_ICCP, WEBP_TAG_SIZE) != WEBP_TAG_SIZE) throw Error(kerImageWriteFailed);
                    ul2Data(data, (uint32_t) iccProfile_.size_, littleEndian);
                    if (outIo.write(data, WEBP_TAG_SIZE) != WEBP_TAG_SIZE) throw Error(kerImageWriteFailed);
                    if (outIo.write(iccProfile_.pData_, iccProfile_.size_) != iccProfile_.size_) {
                        throw Error(kerImageWriteFailed);
                    }
                    has_icc = false;
                }
            } else if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_ICCP)) {
                // Skip it altogether handle it prior to here :)
            } else if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_EXIF)) {
                // Skip and add new data afterwards
            } else if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_XMP)) {
                // Skip and add new data afterwards
            } else {
                if (outIo.write(chunkId.pData_, WEBP_TAG_SIZE) != WEBP_TAG_SIZE)
                    throw Error(kerImageWriteFailed);
                if (outIo.write(size_buff, WEBP_TAG_SIZE) != WEBP_TAG_SIZE)
                    throw Error(kerImageWriteFailed);
                if (outIo.write(payload.pData_, payload.size_) != payload.size_)
                    throw Error(kerImageWriteFailed);
            }

            // Encoder required to pad odd sized data with a null byte
            if (outIo.tell() % 2) {
                if (outIo.write(&WEBP_PAD_ODD, 1) != 1) throw Error(kerImageWriteFailed);
            }
        }

        if (has_exif) {
            if (outIo.write((const byte*)WEBP_CHUNK_HEADER_EXIF, WEBP_TAG_SIZE) != WEBP_TAG_SIZE) throw Error(kerImageWriteFailed);
            us2Data(data, (uint16_t) blob.size()+8, bigEndian);
            ul2Data(data, (uint32_t) blob.size(), littleEndian);
            if (outIo.write(data, WEBP_TAG_SIZE) != WEBP_TAG_SIZE) throw Error(kerImageWriteFailed);
            if (outIo.write((const byte*)&blob[0], static_cast<long>(blob.size())) != (long)blob.size())
            {
                throw Error(kerImageWriteFailed);
            }
            if (outIo.tell() % 2) {
                if (outIo.write(&WEBP_PAD_ODD, 1) != 1) throw Error(kerImageWriteFailed);
            }
        }

        if (has_xmp) {
            if (outIo.write((const byte*)WEBP_CHUNK_HEADER_XMP, WEBP_TAG_SIZE) != WEBP_TAG_SIZE) throw Error(kerImageWriteFailed);
            ul2Data(data, (uint32_t) xmpPacket().size(), littleEndian);
            if (outIo.write(data, WEBP_TAG_SIZE) != WEBP_TAG_SIZE) throw Error(kerImageWriteFailed);
            if (outIo.write((const byte*)xmp.data(), static_cast<long>(xmp.size())) != (long)xmp.size()) {
                throw Error(kerImageWriteFailed);
            }
            if (outIo.tell() % 2) {
                if (outIo.write(&WEBP_PAD_ODD, 1) != 1) throw Error(kerImageWriteFailed);
            }
        }

        // Fix File Size Payload Data
        outIo.seek(0, BasicIo::beg);
        filesize = outIo.size() - 8;
        outIo.seek(4, BasicIo::beg);
        ul2Data(data, (uint32_t) filesize, littleEndian);
        if (outIo.write(data, WEBP_TAG_SIZE) != WEBP_TAG_SIZE) throw Error(kerImageWriteFailed);

    } // WebPImage::writeMetadata