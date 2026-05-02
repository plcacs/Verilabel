int DCTStream::getChars(int nChars, unsigned char *buffer)
{
    for (int i = 0; i < nChars;) {
        if (current == limit) {
            if (!readLine())
                return i;
        }
        int left = limit - current;
        if (left + i > nChars)
            left = nChars - i;
        memcpy(buffer + i, current, left);
        current += left;
        i += left;
    }
    return nChars;
}