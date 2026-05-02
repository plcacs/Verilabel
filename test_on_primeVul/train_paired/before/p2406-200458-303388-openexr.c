MultiPartInputFile::Data::chunkOffsetReconstruction(OPENEXR_IMF_INTERNAL_NAMESPACE::IStream& is, const vector<InputPartData*>& parts)
{
    //
    // Reconstruct broken chunk offset tables. Stop once we received any exception.
    //

    Int64 position = is.tellg();

    
    //
    // check we understand all the parts available: if not, we cannot continue
    // exceptions thrown here should trickle back up to the constructor
    //
    
    for (size_t i = 0; i < parts.size(); i++)
    {
        Header& header=parts[i]->header;
        
        //
        // do we have a valid type entry?
        // we only need them for true multipart files or single part non-image (deep) files
        //
        if(!header.hasType() && (isMultiPart(version) || isNonImage(version)))
        {
            throw IEX_NAMESPACE::ArgExc("cannot reconstruct incomplete file: part with missing type");
        }
        if(!isSupportedType(header.type()))
        {
            throw IEX_NAMESPACE::ArgExc("cannot reconstruct incomplete file: part with unknown type "+header.type());
        }
    }
    
    
    // how many chunks should we read? We should stop when we reach the end
    size_t total_chunks = 0;
        
    // for tiled-based parts, array of (pointers to) tileOffsets objects
    // to create mapping between tile coordinates and chunk table indices
    
    
    vector<TileOffsets*> tileOffsets(parts.size());
    
    // for scanline-based parts, number of scanlines in each chunk
    vector<int> rowsizes(parts.size());
        
    for(size_t i = 0 ; i < parts.size() ; i++)
    {
        total_chunks += parts[i]->chunkOffsets.size();
        if (isTiled(parts[i]->header.type()))
        {
            tileOffsets[i] = createTileOffsets(parts[i]->header);
        }else{
            tileOffsets[i] = NULL;
            // (TODO) fix this so that it doesn't need to be revised for future compression types.
            switch(parts[i]->header.compression())
            {
                case DWAB_COMPRESSION :
                    rowsizes[i] = 256;
                    break;
                case PIZ_COMPRESSION :
                case B44_COMPRESSION :
                case B44A_COMPRESSION :
                case DWAA_COMPRESSION :
                    rowsizes[i]=32;
                    break;
                case ZIP_COMPRESSION :
                case PXR24_COMPRESSION :
                    rowsizes[i]=16;
                    break;
                case ZIPS_COMPRESSION :
                case RLE_COMPRESSION :
                case NO_COMPRESSION :
                    rowsizes[i]=1;
                    break;
                default :
                    throw(IEX_NAMESPACE::ArgExc("Unknown compression method in chunk offset reconstruction"));
            }
        }
     }
        
     try
     {
            
        //
        // 
        //
        
        Int64 chunk_start = position;
        for (size_t i = 0; i < total_chunks ; i++)
        {
            //
            // do we have a part number?
            //
            
            int partNumber = 0;
            if(isMultiPart(version))
            {
                OPENEXR_IMF_INTERNAL_NAMESPACE::Xdr::read <OPENEXR_IMF_INTERNAL_NAMESPACE::StreamIO> (is, partNumber);
            }
            
            
            
            if(partNumber<0 || partNumber> static_cast<int>(parts.size()))
            {
                throw IEX_NAMESPACE::IoExc("part number out of range");
            }
            
            Header& header = parts[partNumber]->header;

            // size of chunk NOT including multipart field
            
            Int64 size_of_chunk=0;

            if (isTiled(header.type()))
            {
                //
                // 
                //
                int tilex,tiley,levelx,levely;
                OPENEXR_IMF_INTERNAL_NAMESPACE::Xdr::read <OPENEXR_IMF_INTERNAL_NAMESPACE::StreamIO> (is, tilex);
                OPENEXR_IMF_INTERNAL_NAMESPACE::Xdr::read <OPENEXR_IMF_INTERNAL_NAMESPACE::StreamIO> (is, tiley);
                OPENEXR_IMF_INTERNAL_NAMESPACE::Xdr::read <OPENEXR_IMF_INTERNAL_NAMESPACE::StreamIO> (is, levelx);
                OPENEXR_IMF_INTERNAL_NAMESPACE::Xdr::read <OPENEXR_IMF_INTERNAL_NAMESPACE::StreamIO> (is, levely);
                
                //std::cout << "chunk_start for " << tilex <<',' << tiley << ',' << levelx << ' ' << levely << ':' << chunk_start << std::endl;
                    
                
                if(!tileOffsets[partNumber])
                {
                    // this shouldn't actually happen - we should have allocated a valid
                    // tileOffsets for any part which isTiled
                    throw IEX_NAMESPACE::IoExc("part not tiled");
                    
                }
                
                if(!tileOffsets[partNumber]->isValidTile(tilex,tiley,levelx,levely))
                {
                    throw IEX_NAMESPACE::IoExc("invalid tile coordinates");
                }
                
                (*tileOffsets[partNumber])(tilex,tiley,levelx,levely)=chunk_start;
                
                // compute chunk sizes - different procedure for deep tiles and regular
                // ones
                if(header.type()==DEEPTILE)
                {
                    Int64 packed_offset;
                    Int64 packed_sample;
                    OPENEXR_IMF_INTERNAL_NAMESPACE::Xdr::read <OPENEXR_IMF_INTERNAL_NAMESPACE::StreamIO> (is, packed_offset);
                    OPENEXR_IMF_INTERNAL_NAMESPACE::Xdr::read <OPENEXR_IMF_INTERNAL_NAMESPACE::StreamIO> (is, packed_sample);
                    
                    //add 40 byte header to packed sizes (tile coordinates, packed sizes, unpacked size)
                    size_of_chunk=packed_offset+packed_sample+40;
                }
                else
                {
                    
                    // regular image has 20 bytes of header, 4 byte chunksize;
                    int chunksize;
                    OPENEXR_IMF_INTERNAL_NAMESPACE::Xdr::read <OPENEXR_IMF_INTERNAL_NAMESPACE::StreamIO> (is, chunksize);
                    size_of_chunk=chunksize+20;
                }
            }
            else
            {
                int y_coordinate;
                OPENEXR_IMF_INTERNAL_NAMESPACE::Xdr::read <OPENEXR_IMF_INTERNAL_NAMESPACE::StreamIO> (is, y_coordinate);
                
                
                if(y_coordinate < header.dataWindow().min.y || y_coordinate > header.dataWindow().max.y)
                {
                   throw IEX_NAMESPACE::IoExc("y out of range");
                }
                y_coordinate -= header.dataWindow().min.y;
                y_coordinate /= rowsizes[partNumber];   
                
                if(y_coordinate < 0 || y_coordinate >= int(parts[partNumber]->chunkOffsets.size()))
                {
                   throw IEX_NAMESPACE::IoExc("chunk index out of range");
                }
                
                parts[partNumber]->chunkOffsets[y_coordinate]=chunk_start;
                
                if(header.type()==DEEPSCANLINE)
                {
                    Int64 packed_offset;
                    Int64 packed_sample;
                    OPENEXR_IMF_INTERNAL_NAMESPACE::Xdr::read <OPENEXR_IMF_INTERNAL_NAMESPACE::StreamIO> (is, packed_offset);
                    OPENEXR_IMF_INTERNAL_NAMESPACE::Xdr::read <OPENEXR_IMF_INTERNAL_NAMESPACE::StreamIO> (is, packed_sample);
                    
                    
                    size_of_chunk=packed_offset+packed_sample+28;
                }
                else
                {
                    int chunksize;
                    OPENEXR_IMF_INTERNAL_NAMESPACE::Xdr::read <OPENEXR_IMF_INTERNAL_NAMESPACE::StreamIO> (is, chunksize);   
                    size_of_chunk=chunksize+8;
                }
                
            }
            
            if(isMultiPart(version))
            {
                chunk_start+=4;
            }
            
            chunk_start+=size_of_chunk;
            
            is.seekg(chunk_start);
            
        }
        
    }
    catch (...)
    {
        //
        // Suppress all exceptions.  This functions is
        // called only to reconstruct the line offset
        // table for incomplete files, and exceptions
        // are likely.
        //
    }

    // copy tiled part data back to chunk offsets
    
    for(size_t partNumber=0;partNumber<parts.size();partNumber++)
    {
        if(tileOffsets[partNumber])
        {
            size_t pos=0;
            vector<vector<vector <Int64> > > offsets = tileOffsets[partNumber]->getOffsets();
            for (size_t l = 0; l < offsets.size(); l++)
                for (size_t y = 0; y < offsets[l].size(); y++)
                    for (size_t x = 0; x < offsets[l][y].size(); x++)
                    {
                        parts[ partNumber ]->chunkOffsets[pos] = offsets[l][y][x];
                        pos++;
                    }
           delete tileOffsets[partNumber];
        }
    }

    is.clear();
    is.seekg (position);
}