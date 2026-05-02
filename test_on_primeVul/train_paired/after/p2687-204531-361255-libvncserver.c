static rfbBool MallocFrameBuffer(rfbClient* client) {
uint64_t allocSize;

  if(client->frameBuffer)
    free(client->frameBuffer);

  /* SECURITY: promote 'width' into uint64_t so that the multiplication does not overflow
     'width' and 'height' are 16-bit integers per RFB protocol design
     SIZE_MAX is the maximum value that can fit into size_t
  */
  allocSize = (uint64_t)client->width * client->height * client->format.bitsPerPixel/8;

  if (allocSize >= SIZE_MAX) {
    rfbClientErr("CRITICAL: cannot allocate frameBuffer, requested size is too large\n");
    return FALSE;
  }

  client->frameBuffer=malloc( (size_t)allocSize );

  if (client->frameBuffer == NULL)
    rfbClientErr("CRITICAL: frameBuffer allocation failed, requested size too large or not enough memory?\n");

  return client->frameBuffer?TRUE:FALSE;
}