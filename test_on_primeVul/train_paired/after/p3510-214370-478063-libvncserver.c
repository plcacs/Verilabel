HandleRFBServerMessage(rfbClient* client)
{
  rfbServerToClientMsg msg;

  if (client->serverPort==-1)
    client->vncRec->readTimestamp = TRUE;
  if (!ReadFromRFBServer(client, (char *)&msg, 1))
    return FALSE;

  switch (msg.type) {

  case rfbSetColourMapEntries:
  {
    /* TODO:
    int i;
    uint16_t rgb[3];
    XColor xc;

    if (!ReadFromRFBServer(client, ((char *)&msg) + 1,
			   sz_rfbSetColourMapEntriesMsg - 1))
      return FALSE;

    msg.scme.firstColour = rfbClientSwap16IfLE(msg.scme.firstColour);
    msg.scme.nColours = rfbClientSwap16IfLE(msg.scme.nColours);

    for (i = 0; i < msg.scme.nColours; i++) {
      if (!ReadFromRFBServer(client, (char *)rgb, 6))
	return FALSE;
      xc.pixel = msg.scme.firstColour + i;
      xc.red = rfbClientSwap16IfLE(rgb[0]);
      xc.green = rfbClientSwap16IfLE(rgb[1]);
      xc.blue = rfbClientSwap16IfLE(rgb[2]);
      xc.flags = DoRed|DoGreen|DoBlue;
      XStoreColor(dpy, cmap, &xc);
    }
    */

    break;
  }

  case rfbFramebufferUpdate:
  {
    rfbFramebufferUpdateRectHeader rect;
    int linesToRead;
    int bytesPerLine;
    int i;

    if (!ReadFromRFBServer(client, ((char *)&msg.fu) + 1,
			   sz_rfbFramebufferUpdateMsg - 1))
      return FALSE;

    msg.fu.nRects = rfbClientSwap16IfLE(msg.fu.nRects);

    for (i = 0; i < msg.fu.nRects; i++) {
      if (!ReadFromRFBServer(client, (char *)&rect, sz_rfbFramebufferUpdateRectHeader))
	return FALSE;

      rect.encoding = rfbClientSwap32IfLE(rect.encoding);
      if (rect.encoding == rfbEncodingLastRect)
	break;

      rect.r.x = rfbClientSwap16IfLE(rect.r.x);
      rect.r.y = rfbClientSwap16IfLE(rect.r.y);
      rect.r.w = rfbClientSwap16IfLE(rect.r.w);
      rect.r.h = rfbClientSwap16IfLE(rect.r.h);


      if (rect.encoding == rfbEncodingXCursor ||
	  rect.encoding == rfbEncodingRichCursor) {

	if (!HandleCursorShape(client,
			       rect.r.x, rect.r.y, rect.r.w, rect.r.h,
			       rect.encoding)) {
	  return FALSE;
	}
	continue;
      }

      if (rect.encoding == rfbEncodingPointerPos) {
	if (!client->HandleCursorPos(client,rect.r.x, rect.r.y)) {
	  return FALSE;
	}
	continue;
      }
      
      if (rect.encoding == rfbEncodingKeyboardLedState) {
          /* OK! We have received a keyboard state message!!! */
          client->KeyboardLedStateEnabled = 1;
          if (client->HandleKeyboardLedState!=NULL)
              client->HandleKeyboardLedState(client, rect.r.x, 0);
          /* stash it for the future */
          client->CurrentKeyboardLedState = rect.r.x;
          continue;
      }

      if (rect.encoding == rfbEncodingNewFBSize) {
	client->width = rect.r.w;
	client->height = rect.r.h;
	client->updateRect.x = client->updateRect.y = 0;
	client->updateRect.w = client->width;
	client->updateRect.h = client->height;
	if (!client->MallocFrameBuffer(client))
	  return FALSE;
	SendFramebufferUpdateRequest(client, 0, 0, rect.r.w, rect.r.h, FALSE);
	rfbClientLog("Got new framebuffer size: %dx%d\n", rect.r.w, rect.r.h);
	continue;
      }

      /* rect.r.w=byte count */
      if (rect.encoding == rfbEncodingSupportedMessages) {
          int loop;
          if (!ReadFromRFBServer(client, (char *)&client->supportedMessages, sz_rfbSupportedMessages))
              return FALSE;

          /* msgs is two sets of bit flags of supported messages client2server[] and server2client[] */
          /* currently ignored by this library */

          rfbClientLog("client2server supported messages (bit flags)\n");
          for (loop=0;loop<32;loop+=8)
            rfbClientLog("%02X: %04x %04x %04x %04x - %04x %04x %04x %04x\n", loop,
                client->supportedMessages.client2server[loop],   client->supportedMessages.client2server[loop+1],
                client->supportedMessages.client2server[loop+2], client->supportedMessages.client2server[loop+3],
                client->supportedMessages.client2server[loop+4], client->supportedMessages.client2server[loop+5],
                client->supportedMessages.client2server[loop+6], client->supportedMessages.client2server[loop+7]);

          rfbClientLog("server2client supported messages (bit flags)\n");
          for (loop=0;loop<32;loop+=8)
            rfbClientLog("%02X: %04x %04x %04x %04x - %04x %04x %04x %04x\n", loop,
                client->supportedMessages.server2client[loop],   client->supportedMessages.server2client[loop+1],
                client->supportedMessages.server2client[loop+2], client->supportedMessages.server2client[loop+3],
                client->supportedMessages.server2client[loop+4], client->supportedMessages.server2client[loop+5],
                client->supportedMessages.server2client[loop+6], client->supportedMessages.server2client[loop+7]);
          continue;
      }

      /* rect.r.w=byte count, rect.r.h=# of encodings */
      if (rect.encoding == rfbEncodingSupportedEncodings) {
          char *buffer;
          buffer = malloc(rect.r.w);
          if (!ReadFromRFBServer(client, buffer, rect.r.w))
          {
              free(buffer);
              return FALSE;
          }

          /* buffer now contains rect.r.h # of uint32_t encodings that the server supports */
          /* currently ignored by this library */
          free(buffer);
          continue;
      }

      /* rect.r.w=byte count */
      if (rect.encoding == rfbEncodingServerIdentity) {
          char *buffer;
          buffer = malloc(rect.r.w+1);
          if (!ReadFromRFBServer(client, buffer, rect.r.w))
          {
              free(buffer);
              return FALSE;
          }
          buffer[rect.r.w]=0; /* null terminate, just in case */
          rfbClientLog("Connected to Server \"%s\"\n", buffer);
          free(buffer);
          continue;
      }

      /* rfbEncodingUltraZip is a collection of subrects.   x = # of subrects, and h is always 0 */
      if (rect.encoding != rfbEncodingUltraZip)
      {
        if ((rect.r.x + rect.r.w > client->width) ||
	    (rect.r.y + rect.r.h > client->height))
	    {
	      rfbClientLog("Rect too large: %dx%d at (%d, %d)\n",
	  	  rect.r.w, rect.r.h, rect.r.x, rect.r.y);
	      return FALSE;
            }

        /* UltraVNC with scaling, will send rectangles with a zero W or H
         *
        if ((rect.encoding != rfbEncodingTight) && 
            (rect.r.h * rect.r.w == 0))
        {
	  rfbClientLog("Zero size rect - ignoring (encoding=%d (0x%08x) %dx, %dy, %dw, %dh)\n", rect.encoding, rect.encoding, rect.r.x, rect.r.y, rect.r.w, rect.r.h);
	  continue;
        }
        */
        
        /* If RichCursor encoding is used, we should prevent collisions
	   between framebuffer updates and cursor drawing operations. */
        client->SoftCursorLockArea(client, rect.r.x, rect.r.y, rect.r.w, rect.r.h);
      }

      switch (rect.encoding) {

      case rfbEncodingRaw: {
	int y=rect.r.y, h=rect.r.h;

	bytesPerLine = rect.r.w * client->format.bitsPerPixel / 8;
	/* RealVNC 4.x-5.x on OSX can induce bytesPerLine==0, 
	   usually during GPU accel. */
	/* Regardless of cause, do not divide by zero. */
	linesToRead = bytesPerLine ? (RFB_BUFFER_SIZE / bytesPerLine) : 0;

	while (h > 0) {
	  if (linesToRead > h)
	    linesToRead = h;

	  if (!ReadFromRFBServer(client, client->buffer,bytesPerLine * linesToRead))
	    return FALSE;

	  CopyRectangle(client, (uint8_t *)client->buffer,
			   rect.r.x, y, rect.r.w,linesToRead);

	  h -= linesToRead;
	  y += linesToRead;

	}
      } break;

      case rfbEncodingCopyRect:
      {
	rfbCopyRect cr;

	if (!ReadFromRFBServer(client, (char *)&cr, sz_rfbCopyRect))
	  return FALSE;

	cr.srcX = rfbClientSwap16IfLE(cr.srcX);
	cr.srcY = rfbClientSwap16IfLE(cr.srcY);

	/* If RichCursor encoding is used, we should extend our
	   "cursor lock area" (previously set to destination
	   rectangle) to the source rectangle as well. */
	client->SoftCursorLockArea(client,
				   cr.srcX, cr.srcY, rect.r.w, rect.r.h);

        if (client->GotCopyRect != NULL) {
          client->GotCopyRect(client, cr.srcX, cr.srcY, rect.r.w, rect.r.h,
              rect.r.x, rect.r.y);
        } else
		CopyRectangleFromRectangle(client,
				   cr.srcX, cr.srcY, rect.r.w, rect.r.h,
				   rect.r.x, rect.r.y);

	break;
      }

      case rfbEncodingRRE:
      {
	switch (client->format.bitsPerPixel) {
	case 8:
	  if (!HandleRRE8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 16:
	  if (!HandleRRE16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 32:
	  if (!HandleRRE32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	}
	break;
      }

      case rfbEncodingCoRRE:
      {
	switch (client->format.bitsPerPixel) {
	case 8:
	  if (!HandleCoRRE8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 16:
	  if (!HandleCoRRE16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 32:
	  if (!HandleCoRRE32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	}
	break;
      }

      case rfbEncodingHextile:
      {
	switch (client->format.bitsPerPixel) {
	case 8:
	  if (!HandleHextile8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 16:
	  if (!HandleHextile16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 32:
	  if (!HandleHextile32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	}
	break;
      }

      case rfbEncodingUltra:
      {
        switch (client->format.bitsPerPixel) {
        case 8:
          if (!HandleUltra8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
            return FALSE;
          break;
        case 16:
          if (!HandleUltra16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
            return FALSE;
          break;
        case 32:
          if (!HandleUltra32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
            return FALSE;
          break;
        }
        break;
      }
      case rfbEncodingUltraZip:
      {
        switch (client->format.bitsPerPixel) {
        case 8:
          if (!HandleUltraZip8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
            return FALSE;
          break;
        case 16:
          if (!HandleUltraZip16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
            return FALSE;
          break;
        case 32:
          if (!HandleUltraZip32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
            return FALSE;
          break;
        }
        break;
      }

#ifdef LIBVNCSERVER_HAVE_LIBZ
      case rfbEncodingZlib:
      {
	switch (client->format.bitsPerPixel) {
	case 8:
	  if (!HandleZlib8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 16:
	  if (!HandleZlib16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 32:
	  if (!HandleZlib32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	}
	break;
     }

#ifdef LIBVNCSERVER_HAVE_LIBJPEG
      case rfbEncodingTight:
      {
	switch (client->format.bitsPerPixel) {
	case 8:
	  if (!HandleTight8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 16:
	  if (!HandleTight16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 32:
	  if (!HandleTight32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	}
	break;
      }
#endif
      case rfbEncodingZRLE:
	/* Fail safe for ZYWRLE unsupport VNC server. */
	client->appData.qualityLevel = 9;
	/* fall through */
      case rfbEncodingZYWRLE:
      {
	switch (client->format.bitsPerPixel) {
	case 8:
	  if (!HandleZRLE8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 16:
	  if (client->si.format.greenMax > 0x1F) {
	    if (!HandleZRLE16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	      return FALSE;
	  } else {
	    if (!HandleZRLE15(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	      return FALSE;
	  }
	  break;
	case 32:
	{
	  uint32_t maxColor=(client->format.redMax<<client->format.redShift)|
		(client->format.greenMax<<client->format.greenShift)|
		(client->format.blueMax<<client->format.blueShift);
	  if ((client->format.bigEndian && (maxColor&0xff)==0) ||
	      (!client->format.bigEndian && (maxColor&0xff000000)==0)) {
	    if (!HandleZRLE24(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	      return FALSE;
	  } else if (!client->format.bigEndian && (maxColor&0xff)==0) {
	    if (!HandleZRLE24Up(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	      return FALSE;
	  } else if (client->format.bigEndian && (maxColor&0xff000000)==0) {
	    if (!HandleZRLE24Down(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	      return FALSE;
	  } else if (!HandleZRLE32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	}
	}
	break;
     }

#endif
#ifdef LIBVNCSERVER_CONFIG_LIBVA
      case rfbEncodingH264:
      {
	if (!HandleH264(client, rect.r.x, rect.r.y, rect.r.w, rect.r.h))
	  return FALSE;
	break;
      }
#endif

      default:
	 {
	   rfbBool handled = FALSE;
	   rfbClientProtocolExtension* e;

	   for(e = rfbClientExtensions; !handled && e; e = e->next)
	     if(e->handleEncoding && e->handleEncoding(client, &rect))
	       handled = TRUE;

	   if(!handled) {
	     rfbClientLog("Unknown rect encoding %d\n",
		 (int)rect.encoding);
	     return FALSE;
	   }
	 }
      }

      /* Now we may discard "soft cursor locks". */
      client->SoftCursorUnlockScreen(client);

      client->GotFrameBufferUpdate(client, rect.r.x, rect.r.y, rect.r.w, rect.r.h);
    }

    if (!SendIncrementalFramebufferUpdateRequest(client))
      return FALSE;

    if (client->FinishedFrameBufferUpdate)
      client->FinishedFrameBufferUpdate(client);

    break;
  }

  case rfbBell:
  {
    client->Bell(client);

    break;
  }

  case rfbServerCutText:
  {
    char *buffer;

    if (!ReadFromRFBServer(client, ((char *)&msg) + 1,
			   sz_rfbServerCutTextMsg - 1))
      return FALSE;

    msg.sct.length = rfbClientSwap32IfLE(msg.sct.length);

    buffer = malloc(msg.sct.length+1);

    if (!ReadFromRFBServer(client, buffer, msg.sct.length))
      return FALSE;

    buffer[msg.sct.length] = 0;

    if (client->GotXCutText)
      client->GotXCutText(client, buffer, msg.sct.length);

    free(buffer);

    break;
  }

  case rfbTextChat:
  {
      char *buffer=NULL;
      if (!ReadFromRFBServer(client, ((char *)&msg) + 1,
                             sz_rfbTextChatMsg- 1))
        return FALSE;
      msg.tc.length = rfbClientSwap32IfLE(msg.sct.length);
      switch(msg.tc.length) {
      case rfbTextChatOpen:
          rfbClientLog("Received TextChat Open\n");
          if (client->HandleTextChat!=NULL)
              client->HandleTextChat(client, (int)rfbTextChatOpen, NULL);
          break;
      case rfbTextChatClose:
          rfbClientLog("Received TextChat Close\n");
         if (client->HandleTextChat!=NULL)
              client->HandleTextChat(client, (int)rfbTextChatClose, NULL);
          break;
      case rfbTextChatFinished:
          rfbClientLog("Received TextChat Finished\n");
         if (client->HandleTextChat!=NULL)
              client->HandleTextChat(client, (int)rfbTextChatFinished, NULL);
          break;
      default:
          buffer=malloc(msg.tc.length+1);
          if (!ReadFromRFBServer(client, buffer, msg.tc.length))
          {
              free(buffer);
              return FALSE;
          }
          /* Null Terminate <just in case> */
          buffer[msg.tc.length]=0;
          rfbClientLog("Received TextChat \"%s\"\n", buffer);
          if (client->HandleTextChat!=NULL)
              client->HandleTextChat(client, (int)msg.tc.length, buffer);
          free(buffer);
          break;
      }
      break;
  }

  case rfbXvp:
  {
    if (!ReadFromRFBServer(client, ((char *)&msg) + 1,
                           sz_rfbXvpMsg -1))
      return FALSE;

    SetClient2Server(client, rfbXvp);
    /* technically, we only care what we can *send* to the server
     * but, we set Server2Client Just in case it ever becomes useful
     */
    SetServer2Client(client, rfbXvp);

    if(client->HandleXvpMsg)
      client->HandleXvpMsg(client, msg.xvp.version, msg.xvp.code);

    break;
  }

  case rfbResizeFrameBuffer:
  {
    if (!ReadFromRFBServer(client, ((char *)&msg) + 1,
                           sz_rfbResizeFrameBufferMsg -1))
      return FALSE;
    client->width = rfbClientSwap16IfLE(msg.rsfb.framebufferWidth);
    client->height = rfbClientSwap16IfLE(msg.rsfb.framebufferHeigth);
    client->updateRect.x = client->updateRect.y = 0;
    client->updateRect.w = client->width;
    client->updateRect.h = client->height;
    if (!client->MallocFrameBuffer(client))
      return FALSE;

    SendFramebufferUpdateRequest(client, 0, 0, client->width, client->height, FALSE);
    rfbClientLog("Got new framebuffer size: %dx%d\n", client->width, client->height);
    break;
  }

  case rfbPalmVNCReSizeFrameBuffer:
  {
    if (!ReadFromRFBServer(client, ((char *)&msg) + 1,
                           sz_rfbPalmVNCReSizeFrameBufferMsg -1))
      return FALSE;
    client->width = rfbClientSwap16IfLE(msg.prsfb.buffer_w);
    client->height = rfbClientSwap16IfLE(msg.prsfb.buffer_h);
    client->updateRect.x = client->updateRect.y = 0;
    client->updateRect.w = client->width;
    client->updateRect.h = client->height;
    if (!client->MallocFrameBuffer(client))
      return FALSE;
    SendFramebufferUpdateRequest(client, 0, 0, client->width, client->height, FALSE);
    rfbClientLog("Got new framebuffer size: %dx%d\n", client->width, client->height);
    break;
  }

  default:
    {
      rfbBool handled = FALSE;
      rfbClientProtocolExtension* e;

      for(e = rfbClientExtensions; !handled && e; e = e->next)
	if(e->handleMessage && e->handleMessage(client, &msg))
	  handled = TRUE;

      if(!handled) {
	char buffer[256];
	rfbClientLog("Unknown message type %d from VNC server\n",msg.type);
	ReadFromRFBServer(client, buffer, 256);
	return FALSE;
      }
    }
  }

  return TRUE;
}