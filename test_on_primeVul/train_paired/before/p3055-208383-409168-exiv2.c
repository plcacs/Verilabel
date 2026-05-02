        Header readHeader(BasicIo& io)
        {
            byte header[2];
            io.read(header, 2);

            ByteOrder byteOrder = invalidByteOrder;
            if (header[0] == 'I' && header[1] == 'I')
                byteOrder = littleEndian;
            else if (header[0] == 'M' && header[1] == 'M')
                byteOrder = bigEndian;

            if (byteOrder == invalidByteOrder)
                return Header();

            byte version[2];
            io.read(version, 2);

            const uint16_t magic = getUShort(version, byteOrder);

            if (magic != 0x2A && magic != 0x2B)
                return Header();

            Header result;

            if (magic == 0x2A)
            {
                byte buffer[4];
                io.read(buffer, 4);

                const uint32_t offset = getULong(buffer, byteOrder);
                result = Header(byteOrder, magic, 4, offset);
            }
            else
            {
                byte buffer[8];
                io.read(buffer, 2);
                const int size = getUShort(buffer, byteOrder);
                assert(size == 8);

                io.read(buffer, 2); // null

                io.read(buffer, 8);
                const uint64_t offset = getULongLong(buffer, byteOrder);

                result = Header(byteOrder, magic, size, offset);
            }

            return result;
        }