    void Image::printIFDStructure(BasicIo& io, std::ostream& out, Exiv2::PrintStructureOption option,uint32_t start,bool bSwap,char c,int depth)
    {
        depth++;
        bool bFirst  = true  ;

        // buffer
        const size_t dirSize = 32;
        DataBuf  dir(dirSize);
        bool bPrint = option == kpsBasic || option == kpsRecursive;

        do {
            // Read top of directory
            io.seek(start,BasicIo::beg);
            io.read(dir.pData_, 2);
            uint16_t   dirLength = byteSwap2(dir,0,bSwap);

            bool tooBig = dirLength > 500;
            if ( tooBig ) throw Error(55);

            if ( bFirst && bPrint ) {
                out << Internal::indent(depth) << Internal::stringFormat("STRUCTURE OF TIFF FILE (%c%c): ",c,c) << io.path() << std::endl;
                if ( tooBig ) out << Internal::indent(depth) << "dirLength = " << dirLength << std::endl;
            }

            // Read the dictionary
            for ( int i = 0 ; i < dirLength ; i ++ ) {
                if ( bFirst && bPrint ) {
                    out << Internal::indent(depth)
                        << " address |    tag                              |     "
                        << " type |    count |    offset | value\n";
                }
                bFirst = false;

                io.read(dir.pData_, 12);
                uint16_t tag    = byteSwap2(dir,0,bSwap);
                uint16_t type   = byteSwap2(dir,2,bSwap);
                uint32_t count  = byteSwap4(dir,4,bSwap);
                uint32_t offset = byteSwap4(dir,8,bSwap);

                // Break for unknown tag types else we may segfault.
                if ( !typeValid(type) ) {
                    std::cerr << "invalid type value detected in Image::printIFDStructure:  " << type << std::endl;
                    start = 0; // break from do loop
                    throw Error(56);
                    break; // break from for loop
                }

                std::string sp  = "" ; // output spacer

                //prepare to print the value
                uint32_t kount  = isPrintXMP(tag,option) ? count // haul in all the data
                                : isPrintICC(tag,option) ? count // ditto
                                : isStringType(type)     ? (count > 32 ? 32 : count) // restrict long arrays
                                : count > 5              ? 5
                                : count
                                ;
                uint32_t pad    = isStringType(type) ? 1 : 0;
                uint32_t size   = isStringType(type) ? 1
                                : is2ByteType(type)  ? 2
                                : is4ByteType(type)  ? 4
                                : is8ByteType(type)  ? 8
                                : 1
                                ;

                // if ( offset > io.size() ) offset = 0; // Denial of service?

                // #55 memory allocation crash test/data/POC8
                long long allocate = (long long) (size*count + pad+20);
                if ( allocate > (long long) io.size() ) {
                    throw Error(57);
                }
                DataBuf  buf((long)allocate);  // allocate a buffer
                std::memcpy(buf.pData_,dir.pData_+8,4);  // copy dir[8:11] into buffer (short strings)
                const bool bOffsetIsPointer = count*size > 4;

                if ( bOffsetIsPointer ) {         // read into buffer
                    size_t   restore = io.tell();  // save
                    io.seek(offset,BasicIo::beg);  // position
                    io.read(buf.pData_,count*size);// read
                    io.seek(restore,BasicIo::beg); // restore
                }

                if ( bPrint ) {
                    const uint32_t address = start + 2 + i*12 ;
                    const std::string offsetString = bOffsetIsPointer?
                        Internal::stringFormat("%10u", offset):
                        "";

                    out << Internal::indent(depth)
                    << Internal::stringFormat("%8u | %#06x %-28s |%10s |%9u |%10s | "
                                              ,address,tag,tagName(tag).c_str(),typeName(type),count,offsetString.c_str());
                    if ( isShortType(type) ){
                        for ( size_t k = 0 ; k < kount ; k++ ) {
                            out << sp << byteSwap2(buf,k*size,bSwap);
                            sp = " ";
                        }
                    } else if ( isLongType(type) ){
                        for ( size_t k = 0 ; k < kount ; k++ ) {
                            out << sp << byteSwap4(buf,k*size,bSwap);
                            sp = " ";
                        }

                    } else if ( isRationalType(type) ){
                        for ( size_t k = 0 ; k < kount ; k++ ) {
                            uint32_t a = byteSwap4(buf,k*size+0,bSwap);
                            uint32_t b = byteSwap4(buf,k*size+4,bSwap);
                            out << sp << a << "/" << b;
                            sp = " ";
                        }
                    } else if ( isStringType(type) ) {
                        out << sp << Internal::binaryToString(buf, kount);
                    }

                    sp = kount == count ? "" : " ...";
                    out << sp << std::endl;

                    if ( option == kpsRecursive && (tag == 0x8769 /* ExifTag */ || tag == 0x014a/*SubIFDs*/  || type == tiffIfd) ) {
                        for ( size_t k = 0 ; k < count ; k++ ) {
                            size_t   restore = io.tell();
                            uint32_t offset = byteSwap4(buf,k*size,bSwap);
                            printIFDStructure(io,out,option,offset,bSwap,c,depth);
                            io.seek(restore,BasicIo::beg);
                        }
                    } else if ( option == kpsRecursive && tag == 0x83bb /* IPTCNAA */ ) {
                        size_t   restore = io.tell();  // save
                        io.seek(offset,BasicIo::beg);  // position
                        byte* bytes=new byte[count] ;  // allocate memory
                        io.read(bytes,count)        ;  // read
                        io.seek(restore,BasicIo::beg); // restore
                        IptcData::printStructure(out,bytes,count,depth);
                        delete[] bytes;                // free
                    }  else if ( option == kpsRecursive && tag == 0x927c /* MakerNote */ && count > 10) {
                        size_t   restore = io.tell();  // save

                        uint32_t jump= 10           ;
                        byte     bytes[20]          ;
                        const char* chars = (const char*) &bytes[0] ;
                        io.seek(offset,BasicIo::beg);  // position
                        io.read(bytes,jump    )     ;  // read
                        bytes[jump]=0               ;
                        if ( ::strcmp("Nikon",chars) == 0 ) {
                            // tag is an embedded tiff
                            byte* bytes=new byte[count-jump] ;  // allocate memory
                            io.read(bytes,count-jump)        ;  // read
                            MemIo memIo(bytes,count-jump)    ;  // create a file
                            printTiffStructure(memIo,out,option,depth);
                            delete[] bytes                   ;  // free
                        } else {
                            // tag is an IFD
                            io.seek(0,BasicIo::beg);  // position
                            printIFDStructure(io,out,option,offset,bSwap,c,depth);
                        }

                        io.seek(restore,BasicIo::beg); // restore
                    }
                }

                if ( isPrintXMP(tag,option) ) {
                    buf.pData_[count]=0;
                    out << (char*) buf.pData_;
                }
                if ( isPrintICC(tag,option) ) {
                    out.write((const char*)buf.pData_,count);
                }
            }
            if ( start ) {
                io.read(dir.pData_, 4);
                start = tooBig ? 0 : byteSwap4(dir,0,bSwap);
            }
        } while (start) ;

        if ( bPrint ) {
            out << Internal::indent(depth) << "END " << io.path() << std::endl;
        }
        out.flush();
        depth--;
    }