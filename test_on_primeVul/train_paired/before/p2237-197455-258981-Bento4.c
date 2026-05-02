AP4_AvccAtom::AP4_AvccAtom(AP4_UI32 size, const AP4_UI08* payload) :
    AP4_Atom(AP4_ATOM_TYPE_AVCC, size)
{
    // make a copy of our configuration bytes
    unsigned int payload_size = size-AP4_ATOM_HEADER_SIZE;
    m_RawBytes.SetData(payload, payload_size);

    // parse the payload
    m_ConfigurationVersion = payload[0];
    m_Profile              = payload[1];
    m_ProfileCompatibility = payload[2];
    m_Level                = payload[3];
    m_NaluLengthSize       = 1+(payload[4]&3);
    AP4_UI08 num_seq_params = payload[5]&31;
    m_SequenceParameters.EnsureCapacity(num_seq_params);
    unsigned int cursor = 6;
    for (unsigned int i=0; i<num_seq_params; i++) {
        m_SequenceParameters.Append(AP4_DataBuffer());
        AP4_UI16 param_length = AP4_BytesToInt16BE(&payload[cursor]);
        m_SequenceParameters[i].SetData(&payload[cursor]+2, param_length);
        cursor += 2+param_length;
    }
    AP4_UI08 num_pic_params = payload[cursor++];
    m_PictureParameters.EnsureCapacity(num_pic_params);
    for (unsigned int i=0; i<num_pic_params; i++) {
        m_PictureParameters.Append(AP4_DataBuffer());
        AP4_UI16 param_length = AP4_BytesToInt16BE(&payload[cursor]);
        m_PictureParameters[i].SetData(&payload[cursor]+2, param_length);
        cursor += 2+param_length;
    }
}