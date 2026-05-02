    DataBuf PngChunk::readRawProfile(const DataBuf& text,bool iTXt)
    {
        DataBuf                 info;
        unsigned char           unhex[103]={0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,1, 2,3,4,5,6,7,8,9,0,0,
            0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,10,11,12,
            13,14,15};
        if (text.size_ == 0) {
            return DataBuf();
        }

        if ( iTXt ) {
            info.alloc(text.size_);
            ::memcpy(info.pData_,text.pData_,text.size_);
            return  info;
        }

        const char *sp  = (char*) text.pData_+1;          // current byte (space pointer)
        const char *eot = (char*) text.pData_+text.size_; // end of text

        if (sp >= eot) {
            return DataBuf();
        }

        // Look for newline
        while (*sp != '\n')
        {
            sp++;
            if ( sp == eot )
            {
                return DataBuf();
            }
        }
        sp++ ; // step over '\n'
        if (sp == eot) {
            return DataBuf();
        }

        // Look for length
        while (*sp == '\0' || *sp == ' ' || *sp == '\n')
        {
            sp++;
            if (sp == eot )
            {
                return DataBuf();
            }
        }

        const char* startOfLength = sp;
        while ('0' <= *sp && *sp <= '9')
        {
            sp++;
            if (sp == eot )
            {
                return DataBuf();
            }
        }
        sp++ ; // step over '\n'
        if (sp == eot) {
            return DataBuf();
        }

        long length = (long) atol(startOfLength);
        enforce(0 <= length && length <= (eot - sp)/2, Exiv2::kerCorruptedMetadata);

        // Allocate space
        if (length == 0)
        {
#ifdef DEBUG
            std::cerr << "Exiv2::PngChunk::readRawProfile: Unable To Copy Raw Profile: invalid profile length\n";
#endif
        }
        info.alloc(length);
        if (info.size_ != length)
        {
#ifdef DEBUG
            std::cerr << "Exiv2::PngChunk::readRawProfile: Unable To Copy Raw Profile: cannot allocate memory\n";
#endif
            return DataBuf();
        }

        // Copy profile, skipping white space and column 1 "=" signs

        unsigned char *dp = (unsigned char*)info.pData_; // decode pointer
        unsigned int nibbles = length * 2;

        for (long i = 0; i < (long) nibbles; i++)
        {
            enforce(sp < eot, Exiv2::kerCorruptedMetadata);
            while (*sp < '0' || (*sp > '9' && *sp < 'a') || *sp > 'f')
            {
                if (*sp == '\0')
                {
#ifdef DEBUG
                    std::cerr << "Exiv2::PngChunk::readRawProfile: Unable To Copy Raw Profile: ran out of data\n";
#endif
                    return DataBuf();
                }

                sp++;
                enforce(sp < eot, Exiv2::kerCorruptedMetadata);
            }

            if (i%2 == 0)
                *dp = (unsigned char) (16*unhex[(int) *sp++]);
            else
                (*dp++) += unhex[(int) *sp++];
        }

        return info;

    } // PngChunk::readRawProfile