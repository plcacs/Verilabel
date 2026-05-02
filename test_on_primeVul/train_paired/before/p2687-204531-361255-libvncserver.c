static rfbBool MallocFrameBuffer(rfbClient* client) {
  if(client->frameBuffer)
    free(client->frameBuffer);
  client->frameBuffer=malloc(client->width*client->height*client->format.bitsPerPixel/8);
  return client->frameBuffer?TRUE:FALSE;
}