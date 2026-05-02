rleUncompress (int inLength, int maxLength, const signed char in[], char out[])
{
    char *outStart = out;

    while (inLength > 0)
    {
	if (*in < 0)
	{
	    int count = -((int)*in++);
	    inLength -= count + 1;

	    if (0 > (maxLength -= count))
		return 0;

        // check the input buffer is big enough to contain
        // 'count' bytes of remaining data
        if (inLength < 0)
          return 0;

        memcpy(out, in, count);
        out += count;
        in  += count;
	}
	else
	{
	    int count = *in++;
	    inLength -= 2;

	    if (0 > (maxLength -= count + 1))
		return 0;

        memset(out, *(char*)in, count+1);
        out += count+1;

	    in++;
	}
    }

    return out - outStart;
}