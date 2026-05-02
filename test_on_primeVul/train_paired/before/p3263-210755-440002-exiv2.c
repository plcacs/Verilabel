    DataBuf PngChunk::parseTXTChunk(const DataBuf& data,
                                    int            keysize,
                                    TxtChunkType   type)
    {
        DataBuf arr;

        if(type == zTXt_Chunk)
        {
            // Extract a deflate compressed Latin-1 text chunk

            // we get the compression method after the key
            const byte* compressionMethod = data.pData_ + keysize + 1;
            if ( *compressionMethod != 0x00 )
            {
                // then it isn't zlib compressed and we are sunk
#ifdef DEBUG
                std::cerr << "Exiv2::PngChunk::parseTXTChunk: Non-standard zTXt compression method.\n";
#endif
                throw Error(kerFailedToReadImageData);
            }

            // compressed string after the compression technique spec
            const byte* compressedText      = data.pData_ + keysize + 2;
            unsigned int compressedTextSize = data.size_  - keysize - 2;

            zlibUncompress(compressedText, compressedTextSize, arr);
        }
        else if(type == tEXt_Chunk)
        {
            // Extract a non-compressed Latin-1 text chunk

            // the text comes after the key, but isn't null terminated
            const byte* text = data.pData_ + keysize + 1;
            long textsize    = data.size_  - keysize - 1;

            arr = DataBuf(text, textsize);
        }
        else if(type == iTXt_Chunk)
        {
            const int nullSeparators = std::count(&data.pData_[keysize+3], &data.pData_[data.size_], '\0');
            enforce(nullSeparators >= 2, Exiv2::kerCorruptedMetadata);

            // Extract a deflate compressed or uncompressed UTF-8 text chunk

            // we get the compression flag after the key
            const byte compressionFlag   = data.pData_[keysize + 1];
            // we get the compression method after the compression flag
            const byte compressionMethod = data.pData_[keysize + 2];

            enforce(compressionFlag == 0x00 || compressionFlag == 0x01, Exiv2::kerCorruptedMetadata);
            enforce(compressionMethod == 0x00, Exiv2::kerCorruptedMetadata);

            // language description string after the compression technique spec
            std::string languageText((const char*)(data.pData_ + keysize + 3));
            unsigned int languageTextSize = static_cast<unsigned int>(languageText.size());
            // translated keyword string after the language description
            std::string translatedKeyText((const char*)(data.pData_ + keysize + 3 + languageTextSize +1));
            unsigned int translatedKeyTextSize = static_cast<unsigned int>(translatedKeyText.size());

            if ( compressionFlag == 0x00 )
            {
                // then it's an uncompressed iTXt chunk
#ifdef DEBUG
                std::cout << "Exiv2::PngChunk::parseTXTChunk: We found an uncompressed iTXt field\n";
#endif

                // the text comes after the translated keyword, but isn't null terminated
                const byte* text = data.pData_ + keysize + 3 + languageTextSize + 1 + translatedKeyTextSize + 1;
                long textsize    = data.size_ - (keysize + 3 + languageTextSize + 1 + translatedKeyTextSize + 1);

                arr.alloc(textsize);
                arr = DataBuf(text, textsize);
            }
            else if ( compressionFlag == 0x01 && compressionMethod == 0x00 )
            {
                // then it's a zlib compressed iTXt chunk
#ifdef DEBUG
                std::cout << "Exiv2::PngChunk::parseTXTChunk: We found a zlib compressed iTXt field\n";
#endif

                // the compressed text comes after the translated keyword, but isn't null terminated
                const byte* compressedText = data.pData_ + keysize + 3 + languageTextSize + 1 + translatedKeyTextSize + 1;
                long compressedTextSize    = data.size_ - (keysize + 3 + languageTextSize + 1 + translatedKeyTextSize + 1);

                zlibUncompress(compressedText, compressedTextSize, arr);
            }
            else
            {
                // then it isn't zlib compressed and we are sunk
#ifdef DEBUG
                std::cerr << "Exiv2::PngChunk::parseTXTChunk: Non-standard iTXt compression method.\n";
#endif
                throw Error(kerFailedToReadImageData);
            }
        }
        else
        {
#ifdef DEBUG
            std::cerr << "Exiv2::PngChunk::parseTXTChunk: We found a field, not expected though\n";
#endif
            throw Error(kerFailedToReadImageData);
        }

        return arr;

    } // PngChunk::parsePngChunk