B44Compressor::B44Compressor
    (const Header &hdr,
     size_t maxScanLineSize,
     size_t numScanLines,
     bool optFlatFields)
:
    Compressor (hdr),
    _maxScanLineSize (maxScanLineSize),
    _optFlatFields (optFlatFields),
    _format (XDR),
    _numScanLines (numScanLines),
    _tmpBuffer (0),
    _outBuffer (0),
    _numChans (0),
    _channels (hdr.channels()),
    _channelData (0)
{
    // TODO: Remove this when we can change the ABI
    (void)_maxScanLineSize;
    //
    // Allocate buffers for compressed an uncompressed pixel data,
    // allocate a set of ChannelData structs to help speed up the
    // compress() and uncompress() functions, below, and determine
    // if uncompressed pixel data should be in native or Xdr format.
    //

    _tmpBuffer = new unsigned short
        [checkArraySize (uiMult (maxScanLineSize, numScanLines),
                         sizeof (unsigned short))];

    const ChannelList &channels = header().channels();
    int numHalfChans = 0;

    for (ChannelList::ConstIterator c = channels.begin();
	 c != channels.end();
	 ++c)
    {
	assert (pixelTypeSize (c.channel().type) % pixelTypeSize (HALF) == 0);
	++_numChans;

	if (c.channel().type == HALF)
	    ++numHalfChans;
    }

    //
    // Compressed data may be larger than the input data
    //

    size_t padding = 12 * numHalfChans * (numScanLines + 3) / 4;

    _outBuffer = new char
        [uiAdd (uiMult (maxScanLineSize, numScanLines), padding)];

    _channelData = new ChannelData[_numChans];

    int i = 0;

    for (ChannelList::ConstIterator c = channels.begin();
	 c != channels.end();
	 ++c, ++i)
    {
	_channelData[i].ys = c.channel().ySampling;
	_channelData[i].type = c.channel().type;
	_channelData[i].pLinear = c.channel().pLinear;
	_channelData[i].size =
	    pixelTypeSize (c.channel().type) / pixelTypeSize (HALF);
    }

    const Box2i &dataWindow = hdr.dataWindow();

    _minX = dataWindow.min.x;
    _maxX = dataWindow.max.x;
    _maxY = dataWindow.max.y;

    //
    // We can support uncompressed data in the machine's native
    // format only if all image channels are of type HALF.
    //

    assert (sizeof (unsigned short) == pixelTypeSize (HALF));

    if (_numChans == numHalfChans)
	_format = NATIVE;
}