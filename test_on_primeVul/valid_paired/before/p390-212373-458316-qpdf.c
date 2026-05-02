Pl_ASCIIHexDecoder::flush()
{
    if (this->pos == 0)
    {
	QTC::TC("libtests", "Pl_ASCIIHexDecoder no-op flush");
	return;
    }
    int b[2];
    for (int i = 0; i < 2; ++i)
    {
	if (this->inbuf[i] >= 'A')
	{
	    b[i] = this->inbuf[i] - 'A' + 10;
	}
	else
	{
	    b[i] = this->inbuf[i] - '0';
	}
    }
    unsigned char ch = static_cast<unsigned char>((b[0] << 4) + b[1]);

    QTC::TC("libtests", "Pl_ASCIIHexDecoder partial flush",
	    (this->pos == 2) ? 0 : 1);
    getNext()->write(&ch, 1);

    this->pos = 0;
    this->inbuf[0] = '0';
    this->inbuf[1] = '0';
    this->inbuf[2] = '\0';
}