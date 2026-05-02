CompositeDeepScanLine::setFrameBuffer(const FrameBuffer& fr)
{
    
    //
    // count channels; build map between channels in frame buffer
    // and channels in internal buffers
    //
    
    _Data->_channels.resize(3);
    _Data->_channels[0]="Z";
    _Data->_channels[1]=_Data->_zback ? "ZBack" : "Z";
    _Data->_channels[2]="A";
    _Data->_bufferMap.resize(0);
    
    for(FrameBuffer::ConstIterator q=fr.begin();q!=fr.end();q++)
    {
        string name(q.name());
        if(name=="ZBack")
        {
            _Data->_bufferMap.push_back(1);
        }else if(name=="Z")
        {
            _Data->_bufferMap.push_back(0);
        }else if(name=="A")
        {
            _Data->_bufferMap.push_back(2);
        }else{
            _Data->_bufferMap.push_back(static_cast<int>(_Data->_channels.size()));
            _Data->_channels.push_back(name);
        }
    }
    
  _Data->_outputFrameBuffer=fr;
}