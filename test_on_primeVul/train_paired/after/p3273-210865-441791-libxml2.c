xmlDictComputeFastQKey(const xmlChar *prefix, int plen,
                       const xmlChar *name, int len, int seed)
{
    unsigned long value = (unsigned long) seed;

    if (plen == 0)
	value += 30 * (unsigned long) ':';
    else
	value += 30 * (*prefix);

    if (len > 10) {
        int offset = len - (plen + 1 + 1);
	if (offset < 0)
	    offset = len - (10 + 1);
	value += name[offset];
        len = 10;
	if (plen > 10)
	    plen = 10;
    }
    switch (plen) {
        case 10: value += prefix[9];
        case 9: value += prefix[8];
        case 8: value += prefix[7];
        case 7: value += prefix[6];
        case 6: value += prefix[5];
        case 5: value += prefix[4];
        case 4: value += prefix[3];
        case 3: value += prefix[2];
        case 2: value += prefix[1];
        case 1: value += prefix[0];
        default: break;
    }
    len -= plen;
    if (len > 0) {
        value += (unsigned long) ':';
	len--;
    }
    switch (len) {
        case 10: value += name[9];
        case 9: value += name[8];
        case 8: value += name[7];
        case 7: value += name[6];
        case 6: value += name[5];
        case 5: value += name[4];
        case 4: value += name[3];
        case 3: value += name[2];
        case 2: value += name[1];
        case 1: value += name[0];
        default: break;
    }
    return(value);
}