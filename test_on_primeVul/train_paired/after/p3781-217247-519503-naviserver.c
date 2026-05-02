ChunkedDecode(Request *reqPtr, bool update)
{
    const Tcl_DString *bufPtr;
    const char        *end, *chunkStart;
    SockState         result = SOCK_READY;

    NS_NONNULL_ASSERT(reqPtr != NULL);

    bufPtr = &reqPtr->buffer;
    end = bufPtr->string + bufPtr->length;
    chunkStart = bufPtr->string + reqPtr->chunkStartOff;

    while (reqPtr->chunkStartOff <  (size_t)bufPtr->length) {
        char   *p = strstr(chunkStart, "\r\n");
        long    chunkLength;

        if (p == NULL) {
            Ns_Log(DriverDebug, "ChunkedDecode: chunk did not find end-of-line");
            result = SOCK_MORE;
            break;
        }

        *p = '\0';
        chunkLength = strtol(chunkStart, NULL, 16);
        *p = '\r';
        if (chunkLength < 0) {
            Ns_Log(Warning, "ChunkedDecode: negative chunk length");
            result = SOCK_BADREQUEST;
            break;
        }
        *p = '\r';

        if (p + 2 + chunkLength > end) {
            Ns_Log(DriverDebug, "ChunkedDecode: chunk length past end of buffer");
            result = SOCK_MORE;
            break;
        }
        if (update) {
            char *writeBuffer = bufPtr->string + reqPtr->chunkWriteOff;

            memmove(writeBuffer, p + 2, (size_t)chunkLength);
            reqPtr->chunkWriteOff += (size_t)chunkLength;
            *(writeBuffer + chunkLength) = '\0';
        }
        reqPtr->chunkStartOff += (size_t)(p - chunkStart) + 4u + (size_t)chunkLength;
        chunkStart = bufPtr->string + reqPtr->chunkStartOff;
    }

    return result;
}