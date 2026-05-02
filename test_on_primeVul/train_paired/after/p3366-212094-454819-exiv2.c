    void PsdImage::readMetadata()
    {
#ifdef DEBUG
        std::cerr << "Exiv2::PsdImage::readMetadata: Reading Photoshop file " << io_->path() << "\n";
#endif
        if (io_->open() != 0)
        {
            throw Error(kerDataSourceOpenFailed, io_->path(), strError());
        }
        IoCloser closer(*io_);
        // Ensure that this is the correct image type
        if (!isPsdType(*io_, false))
        {
            if (io_->error() || io_->eof()) throw Error(kerFailedToReadImageData);
            throw Error(kerNotAnImage, "Photoshop");
        }
        clearMetadata();

        /*
          The Photoshop header goes as follows -- all numbers are in big-endian byte order:

          offset  length   name       description
          ======  =======  =========  =========
           0      4 bytes  signature  always '8BPS'
           4      2 bytes  version    always equal to 1
           6      6 bytes  reserved   must be zero
          12      2 bytes  channels   number of channels in the image, including alpha channels (1 to 24)
          14      4 bytes  rows       the height of the image in pixels
          18      4 bytes  columns    the width of the image in pixels
          22      2 bytes  depth      the number of bits per channel
          24      2 bytes  mode       the color mode of the file; Supported values are: Bitmap=0; Grayscale=1; Indexed=2; RGB=3; CMYK=4; Multichannel=7; Duotone=8; Lab=9
        */
        byte buf[26];
        if (io_->read(buf, 26) != 26)
        {
            throw Error(kerNotAnImage, "Photoshop");
        }
        pixelWidth_ = getLong(buf + 18, bigEndian);
        pixelHeight_ = getLong(buf + 14, bigEndian);

        // immediately following the image header is the color mode data section,
        // the first four bytes of which specify the byte size of the whole section
        if (io_->read(buf, 4) != 4)
        {
            throw Error(kerNotAnImage, "Photoshop");
        }

        // skip it
        uint32_t colorDataLength = getULong(buf, bigEndian);
        if (io_->seek(colorDataLength, BasicIo::cur))
        {
            throw Error(kerNotAnImage, "Photoshop");
        }

        // after the color data section, comes a list of resource blocks, preceded by the total byte size
        if (io_->read(buf, 4) != 4)
        {
            throw Error(kerNotAnImage, "Photoshop");
        }
        uint32_t resourcesLength = getULong(buf, bigEndian);
        enforce(resourcesLength < io_->size(), Exiv2::kerCorruptedMetadata);

        while (resourcesLength > 0)
        {
            if (io_->read(buf, 8) != 8)
            {
                throw Error(kerNotAnImage, "Photoshop");
            }

            if (!Photoshop::isIrb(buf, 4))
            {
                break; // bad resource type
            }
            uint16_t resourceId = getUShort(buf + 4, bigEndian);
            uint32_t resourceNameLength = buf[6] & ~1;

            // skip the resource name, plus any padding
            io_->seek(resourceNameLength, BasicIo::cur);

            // read resource size
            if (io_->read(buf, 4) != 4)
            {
                throw Error(kerNotAnImage, "Photoshop");
            }
            uint32_t resourceSize = getULong(buf, bigEndian);
            uint32_t curOffset = io_->tell();

#ifdef DEBUG
        std::cerr << std::hex << "resourceId: " << resourceId << std::dec << " length: " << resourceSize << std::hex << "\n";
#endif

            readResourceBlock(resourceId, resourceSize);
            resourceSize = (resourceSize + 1) & ~1;        // pad to even
            io_->seek(curOffset + resourceSize, BasicIo::beg);
            resourcesLength -= Safe::add(Safe::add(static_cast<uint32_t>(12), resourceNameLength),
                                         resourceSize);
        }

    } // PsdImage::readMetadata