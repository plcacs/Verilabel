Pl_ASCII85Decoder::flush()
{
    if (this->pos == 0)
    {
	QTC::TC("libtests", "Pl_ASCII85Decoder no-op flush");
	return;
    }
    unsigned long lval = 0;
    for (int i = 0; i < 5; ++i)
    {
	lval *= 85;
	lval += (this->inbuf[i] - 33U);
    }

    unsigned char outbuf[4];
    memset(outbuf, 0, 4);
    for (int i = 3; i >= 0; --i)
    {
	outbuf[i] = lval & 0xff;
	lval >>= 8;
    }

    QTC::TC("libtests", "Pl_ASCII85Decoder partial flush",
	    (this->pos == 5) ? 0 : 1);
    getNext()->write(outbuf, this->pos - 1);

    this->pos = 0;
    memset(this->inbuf, 117, 5);
}