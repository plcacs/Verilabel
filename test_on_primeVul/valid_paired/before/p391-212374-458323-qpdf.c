Pl_AES_PDF::flush(bool strip_padding)
{
    assert(this->offset == this->buf_size);

    if (first)
    {
	first = false;
        bool return_after_init = false;
	if (this->cbc_mode)
	{
	    if (encrypt)
	    {
		// Set cbc_block to the initialization vector, and if
		// not zero, write it to the output stream.
		initializeVector();
                if (! (this->use_zero_iv || this->use_specified_iv))
                {
                    getNext()->write(this->cbc_block, this->buf_size);
                }
	    }
	    else if (this->use_zero_iv || this->use_specified_iv)
            {
                // Initialize vector with zeroes; zero vector was not
                // written to the beginning of the input file.
                initializeVector();
            }
            else
	    {
		// Take the first block of input as the initialization
		// vector.  There's nothing to write at this time.
		memcpy(this->cbc_block, this->inbuf, this->buf_size);
		this->offset = 0;
                return_after_init = true;
	    }
	}
        this->crypto->rijndael_init(
            encrypt, this->key.get(), key_bytes,
            this->cbc_mode, this->cbc_block);
        if (return_after_init)
        {
            return;
        }
    }

    if (this->encrypt)
    {
	this->crypto->rijndael_process(this->inbuf, this->outbuf);
    }
    else
    {
	this->crypto->rijndael_process(this->inbuf, this->outbuf);
    }
    unsigned int bytes = this->buf_size;
    if (strip_padding)
    {
	unsigned char last = this->outbuf[this->buf_size - 1];
	if (last <= this->buf_size)
	{
	    bool strip = true;
	    for (unsigned int i = 1; i <= last; ++i)
	    {
		if (this->outbuf[this->buf_size - i] != last)
		{
		    strip = false;
		    break;
		}
	    }
	    if (strip)
	    {
		bytes -= last;
	    }
	}
    }
    getNext()->write(this->outbuf, bytes);
    this->offset = 0;
}