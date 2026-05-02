ChunkedDecode(Request *reqPtr, bool update)
{
    const Tcl_DString *bufPtr;
    const char        *end, *chunkStart;
    bool              success = NS_TRUE;

    NS_NONNULL_ASSERT(reqPtr != NULL);

    bufPtr = &reqPtr->buffer;
    end = bufPtr->string + bufPtr->length;
    chunkStart = bufPtr->string + reqPtr->chunkStartOff;

    while (reqPtr->chunkStartOff <  (size_t)bufPtr->length) {
        char   *p = strstr(chunkStart, "\r\n");
        size_t  chunk_length;

        if (p == NULL) {
            Ns_Log(DriverDebug, "ChunkedDecode: chunk did not find end-of-line");
            success = NS_FALSE;
            break;
        }

        *p = '\0';
        chunk_length = (size_t)strtol(chunkStart, NULL, 16);
        *p = '\r';

        if (p + 2 + chunk_length > end) {
            Ns_Log(DriverDebug, "ChunkedDecode: chunk length past end of buffer");
            success = NS_FALSE;
            break;
        }
        if (update) {
            char *writeBuffer = bufPtr->string + reqPtr->chunkWriteOff;

            memmove(writeBuffer, p + 2, chunk_length);
            reqPtr->chunkWriteOff += chunk_length;
            *(writeBuffer + chunk_length) = '\0';
        }
        reqPtr->chunkStartOff += (size_t)(p - chunkStart) + 4u + chunk_length;
        chunkStart = bufPtr->string + reqPtr->chunkStartOff;
    }

    return success;
}