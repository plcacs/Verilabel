AP4_HvccAtom::AP4_HvccAtom(AP4_UI32 size, const AP4_UI08* payload) :
    AP4_Atom(AP4_ATOM_TYPE_HVCC, size)
{
    // make a copy of our configuration bytes
    unsigned int payload_size = size-AP4_ATOM_HEADER_SIZE;
    m_RawBytes.SetData(payload, payload_size);

    // parse the payload
    m_ConfigurationVersion   = payload[0];
    m_GeneralProfileSpace    = (payload[1]>>6) & 0x03;
    m_GeneralTierFlag        = (payload[1]>>5) & 0x01;
    m_GeneralProfile         = (payload[1]   ) & 0x1F;
    m_GeneralProfileCompatibilityFlags = AP4_BytesToUInt32BE(&payload[2]);
    m_GeneralConstraintIndicatorFlags  = (((AP4_UI64)AP4_BytesToUInt32BE(&payload[6]))<<16) | AP4_BytesToUInt16BE(&payload[10]);
    m_GeneralLevel           = payload[12];
    m_Reserved1              = (payload[13]>>4) & 0x0F;
    m_MinSpatialSegmentation = AP4_BytesToUInt16BE(&payload[13]) & 0x0FFF;
    m_Reserved2              = (payload[15]>>2) & 0x3F;
    m_ParallelismType        = payload[15] & 0x03;
    m_Reserved3              = (payload[16]>>2) & 0x3F;
    m_ChromaFormat           = payload[16] & 0x03;
    m_Reserved4              = (payload[17]>>3) & 0x1F;
    m_LumaBitDepth           = 8+(payload[17] & 0x07);
    m_Reserved5              = (payload[18]>>3) & 0x1F;
    m_ChromaBitDepth         = 8+(payload[18] & 0x07);
    m_AverageFrameRate       = AP4_BytesToUInt16BE(&payload[19]);
    m_ConstantFrameRate      = (payload[21]>>6) & 0x03;
    m_NumTemporalLayers      = (payload[21]>>3) & 0x07;
    m_TemporalIdNested       = (payload[21]>>2) & 0x01;
    m_NaluLengthSize         = 1+(payload[21] & 0x03);
    
    AP4_UI08 num_seq = payload[22];
    m_Sequences.SetItemCount(num_seq);
    unsigned int cursor = 23;
    for (unsigned int i=0; i<num_seq; i++) {

        Sequence& seq = m_Sequences[i];
        if (cursor+1 > payload_size) break;
        seq.m_ArrayCompleteness = (payload[cursor] >> 7) & 0x01;
        seq.m_Reserved          = (payload[cursor] >> 6) & 0x01;
        seq.m_NaluType          = payload[cursor] & 0x3F;
        cursor += 1;
        
        if (cursor+2 > payload_size) break;
        AP4_UI16 nalu_count = AP4_BytesToUInt16BE(&payload[cursor]);
        seq.m_Nalus.SetItemCount(nalu_count);
        cursor += 2;
        
        for (unsigned int j=0; j<nalu_count; j++) {
            if (cursor+2 > payload_size) break;
            unsigned int nalu_length = AP4_BytesToUInt16BE(&payload[cursor]);
            cursor += 2;
            if (cursor + nalu_length > payload_size) break;
            seq.m_Nalus[j].SetData(&payload[cursor], nalu_length);
            cursor += nalu_length;
        }
    }
}