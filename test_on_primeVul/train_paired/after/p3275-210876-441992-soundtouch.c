void WavOutFile::write(const float *buffer, int numElems)
{
    int numBytes;
    int bytesPerSample;

    if (numElems == 0) return;

    bytesPerSample = header.format.bits_per_sample / 8;
    numBytes = numElems * bytesPerSample;
    void *temp = getConvBuffer(numBytes + 7);   // round bit up to avoid buffer overrun with 24bit-value assignment

    switch (bytesPerSample)
    {
        case 1:
        {
            unsigned char *temp2 = (unsigned char *)temp;
            for (int i = 0; i < numElems; i ++)
            {
                temp2[i] = (unsigned char)saturate(buffer[i] * 128.0f + 128.0f, 0.0f, 255.0f);
            }
            break;
        }

        case 2:
        {
            short *temp2 = (short *)temp;
            for (int i = 0; i < numElems; i ++)
            {
                short value = (short)saturate(buffer[i] * 32768.0f, -32768.0f, 32767.0f);
                temp2[i] = _swap16(value);
            }
            break;
        }

        case 3:
        {
            char *temp2 = (char *)temp;
            for (int i = 0; i < numElems; i ++)
            {
                int value = saturate(buffer[i] * 8388608.0f, -8388608.0f, 8388607.0f);
                *((int*)temp2) = _swap32(value);
                temp2 += 3;
            }
            break;
        }

        case 4:
        {
            int *temp2 = (int *)temp;
            for (int i = 0; i < numElems; i ++)
            {
                int value = saturate(buffer[i] * 2147483648.0f, -2147483648.0f, 2147483647.0f);
                temp2[i] = _swap32(value);
            }
            break;
        }

        default:
            assert(false);
    }

    int res = (int)fwrite(temp, 1, numBytes, fptr);

    if (res != numBytes) 
    {
        ST_THROW_RT_ERROR("Error while writing to a wav file.");
    }
    bytesWritten += numBytes;
}