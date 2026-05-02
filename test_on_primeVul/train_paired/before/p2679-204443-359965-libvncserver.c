rfbProcessClientNormalMessage(rfbClientPtr cl)
{
    int n=0;
    rfbClientToServerMsg msg;
    char *str;
    int i;
    uint32_t enc=0;
    uint32_t lastPreferredEncoding = -1;
    char encBuf[64];
    char encBuf2[64];

#ifdef LIBVNCSERVER_WITH_WEBSOCKETS
    if (cl->wsctx && webSocketCheckDisconnect(cl))
      return;
#endif

    if ((n = rfbReadExact(cl, (char *)&msg, 1)) <= 0) {
        if (n != 0)
            rfbLogPerror("rfbProcessClientNormalMessage: read");
        rfbCloseClient(cl);
        return;
    }

    switch (msg.type) {

    case rfbSetPixelFormat:

        if ((n = rfbReadExact(cl, ((char *)&msg) + 1,
                           sz_rfbSetPixelFormatMsg - 1)) <= 0) {
            if (n != 0)
                rfbLogPerror("rfbProcessClientNormalMessage: read");
            rfbCloseClient(cl);
            return;
        }

        cl->format.bitsPerPixel = msg.spf.format.bitsPerPixel;
        cl->format.depth = msg.spf.format.depth;
        cl->format.bigEndian = (msg.spf.format.bigEndian ? TRUE : FALSE);
        cl->format.trueColour = (msg.spf.format.trueColour ? TRUE : FALSE);
        cl->format.redMax = Swap16IfLE(msg.spf.format.redMax);
        cl->format.greenMax = Swap16IfLE(msg.spf.format.greenMax);
        cl->format.blueMax = Swap16IfLE(msg.spf.format.blueMax);
        cl->format.redShift = msg.spf.format.redShift;
        cl->format.greenShift = msg.spf.format.greenShift;
        cl->format.blueShift = msg.spf.format.blueShift;

	cl->readyForSetColourMapEntries = TRUE;
        cl->screen->setTranslateFunction(cl);

        rfbStatRecordMessageRcvd(cl, msg.type, sz_rfbSetPixelFormatMsg, sz_rfbSetPixelFormatMsg);

        return;


    case rfbFixColourMapEntries:
        if ((n = rfbReadExact(cl, ((char *)&msg) + 1,
                           sz_rfbFixColourMapEntriesMsg - 1)) <= 0) {
            if (n != 0)
                rfbLogPerror("rfbProcessClientNormalMessage: read");
            rfbCloseClient(cl);
            return;
        }
        rfbStatRecordMessageRcvd(cl, msg.type, sz_rfbSetPixelFormatMsg, sz_rfbSetPixelFormatMsg);
        rfbLog("rfbProcessClientNormalMessage: %s",
                "FixColourMapEntries unsupported\n");
        rfbCloseClient(cl);
        return;


    /* NOTE: Some clients send us a set of encodings (ie: PointerPos) designed to enable/disable features...
     * We may want to look into this...
     * Example:
     *     case rfbEncodingXCursor:
     *         cl->enableCursorShapeUpdates = TRUE;
     *
     * Currently: cl->enableCursorShapeUpdates can *never* be turned off...
     */
    case rfbSetEncodings:
    {

        if ((n = rfbReadExact(cl, ((char *)&msg) + 1,
                           sz_rfbSetEncodingsMsg - 1)) <= 0) {
            if (n != 0)
                rfbLogPerror("rfbProcessClientNormalMessage: read");
            rfbCloseClient(cl);
            return;
        }

        msg.se.nEncodings = Swap16IfLE(msg.se.nEncodings);

        rfbStatRecordMessageRcvd(cl, msg.type, sz_rfbSetEncodingsMsg+(msg.se.nEncodings*4),sz_rfbSetEncodingsMsg+(msg.se.nEncodings*4));

        /*
         * UltraVNC Client has the ability to adapt to changing network environments
         * So, let's give it a change to tell us what it wants now!
         */
        if (cl->preferredEncoding!=-1)
            lastPreferredEncoding = cl->preferredEncoding;

        /* Reset all flags to defaults (allows us to switch between PointerPos and Server Drawn Cursors) */
        cl->preferredEncoding=-1;
        cl->useCopyRect              = FALSE;
        cl->useNewFBSize             = FALSE;
        cl->cursorWasChanged         = FALSE;
        cl->useRichCursorEncoding    = FALSE;
        cl->enableCursorPosUpdates   = FALSE;
        cl->enableCursorShapeUpdates = FALSE;
        cl->enableCursorShapeUpdates = FALSE;
        cl->enableLastRectEncoding   = FALSE;
        cl->enableKeyboardLedState   = FALSE;
        cl->enableSupportedMessages  = FALSE;
        cl->enableSupportedEncodings = FALSE;
        cl->enableServerIdentity     = FALSE;
#if defined(LIBVNCSERVER_HAVE_LIBZ) || defined(LIBVNCSERVER_HAVE_LIBPNG)
        cl->tightQualityLevel        = -1;
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
        cl->tightCompressLevel       = TIGHT_DEFAULT_COMPRESSION;
        cl->turboSubsampLevel        = TURBO_DEFAULT_SUBSAMP;
        cl->turboQualityLevel        = -1;
#endif
#endif


        for (i = 0; i < msg.se.nEncodings; i++) {
            if ((n = rfbReadExact(cl, (char *)&enc, 4)) <= 0) {
                if (n != 0)
                    rfbLogPerror("rfbProcessClientNormalMessage: read");
                rfbCloseClient(cl);
                return;
            }
            enc = Swap32IfLE(enc);

            switch (enc) {

            case rfbEncodingCopyRect:
		cl->useCopyRect = TRUE;
                break;
            case rfbEncodingRaw:
            case rfbEncodingRRE:
            case rfbEncodingCoRRE:
            case rfbEncodingHextile:
            case rfbEncodingUltra:
#ifdef LIBVNCSERVER_HAVE_LIBZ
	    case rfbEncodingZlib:
            case rfbEncodingZRLE:
            case rfbEncodingZYWRLE:
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
	    case rfbEncodingTight:
#endif
#endif
#ifdef LIBVNCSERVER_HAVE_LIBPNG
	    case rfbEncodingTightPng:
#endif
            /* The first supported encoding is the 'preferred' encoding */
                if (cl->preferredEncoding == -1)
                    cl->preferredEncoding = enc;


                break;
	    case rfbEncodingXCursor:
		if(!cl->screen->dontConvertRichCursorToXCursor) {
		    rfbLog("Enabling X-style cursor updates for client %s\n",
			   cl->host);
		    /* if cursor was drawn, hide the cursor */
		    if(!cl->enableCursorShapeUpdates)
		        rfbRedrawAfterHideCursor(cl,NULL);

		    cl->enableCursorShapeUpdates = TRUE;
		    cl->cursorWasChanged = TRUE;
		}
		break;
	    case rfbEncodingRichCursor:
	        rfbLog("Enabling full-color cursor updates for client %s\n",
		       cl->host);
		/* if cursor was drawn, hide the cursor */
		if(!cl->enableCursorShapeUpdates)
		    rfbRedrawAfterHideCursor(cl,NULL);

	        cl->enableCursorShapeUpdates = TRUE;
	        cl->useRichCursorEncoding = TRUE;
	        cl->cursorWasChanged = TRUE;
	        break;
	    case rfbEncodingPointerPos:
		if (!cl->enableCursorPosUpdates) {
		    rfbLog("Enabling cursor position updates for client %s\n",
			   cl->host);
		    cl->enableCursorPosUpdates = TRUE;
		    cl->cursorWasMoved = TRUE;
		}
	        break;
	    case rfbEncodingLastRect:
		if (!cl->enableLastRectEncoding) {
		    rfbLog("Enabling LastRect protocol extension for client "
			   "%s\n", cl->host);
		    cl->enableLastRectEncoding = TRUE;
		}
		break;
	    case rfbEncodingNewFBSize:
		if (!cl->useNewFBSize) {
		    rfbLog("Enabling NewFBSize protocol extension for client "
			   "%s\n", cl->host);
		    cl->useNewFBSize = TRUE;
		}
		break;
            case rfbEncodingKeyboardLedState:
                if (!cl->enableKeyboardLedState) {
                  rfbLog("Enabling KeyboardLedState protocol extension for client "
                          "%s\n", cl->host);
                  cl->enableKeyboardLedState = TRUE;
                }
                break;           
            case rfbEncodingSupportedMessages:
                if (!cl->enableSupportedMessages) {
                  rfbLog("Enabling SupportedMessages protocol extension for client "
                          "%s\n", cl->host);
                  cl->enableSupportedMessages = TRUE;
                }
                break;           
            case rfbEncodingSupportedEncodings:
                if (!cl->enableSupportedEncodings) {
                  rfbLog("Enabling SupportedEncodings protocol extension for client "
                          "%s\n", cl->host);
                  cl->enableSupportedEncodings = TRUE;
                }
                break;           
            case rfbEncodingServerIdentity:
                if (!cl->enableServerIdentity) {
                  rfbLog("Enabling ServerIdentity protocol extension for client "
                          "%s\n", cl->host);
                  cl->enableServerIdentity = TRUE;
                }
                break;
	    case rfbEncodingXvp:
	        rfbLog("Enabling Xvp protocol extension for client "
		        "%s\n", cl->host);
		if (!rfbSendXvp(cl, 1, rfbXvp_Init)) {
		  rfbCloseClient(cl);
		  return;
		}
                break;
            default:
#if defined(LIBVNCSERVER_HAVE_LIBZ) || defined(LIBVNCSERVER_HAVE_LIBPNG)
		if ( enc >= (uint32_t)rfbEncodingCompressLevel0 &&
		     enc <= (uint32_t)rfbEncodingCompressLevel9 ) {
		    cl->zlibCompressLevel = enc & 0x0F;
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
		    cl->tightCompressLevel = enc & 0x0F;
		    rfbLog("Using compression level %d for client %s\n",
			   cl->tightCompressLevel, cl->host);
#endif
		} else if ( enc >= (uint32_t)rfbEncodingQualityLevel0 &&
			    enc <= (uint32_t)rfbEncodingQualityLevel9 ) {
		    cl->tightQualityLevel = enc & 0x0F;
		    rfbLog("Using image quality level %d for client %s\n",
			   cl->tightQualityLevel, cl->host);
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
		    cl->turboQualityLevel = tight2turbo_qual[enc & 0x0F];
		    cl->turboSubsampLevel = tight2turbo_subsamp[enc & 0x0F];
		    rfbLog("Using JPEG subsampling %d, Q%d for client %s\n",
			   cl->turboSubsampLevel, cl->turboQualityLevel, cl->host);
		} else if ( enc >= (uint32_t)rfbEncodingFineQualityLevel0 + 1 &&
			    enc <= (uint32_t)rfbEncodingFineQualityLevel100 ) {
		    cl->turboQualityLevel = enc & 0xFF;
		    rfbLog("Using fine quality level %d for client %s\n",
			   cl->turboQualityLevel, cl->host);
		} else if ( enc >= (uint32_t)rfbEncodingSubsamp1X &&
			    enc <= (uint32_t)rfbEncodingSubsampGray ) {
		    cl->turboSubsampLevel = enc & 0xFF;
		    rfbLog("Using subsampling level %d for client %s\n",
			   cl->turboSubsampLevel, cl->host);
#endif
		} else
#endif
		{
			rfbExtensionData* e;
			for(e = cl->extensions; e;) {
				rfbExtensionData* next = e->next;
				if(e->extension->enablePseudoEncoding &&
					e->extension->enablePseudoEncoding(cl,
						&e->data, (int)enc))
					/* ext handles this encoding */
					break;
				e = next;
			}
			if(e == NULL) {
				rfbBool handled = FALSE;
				/* if the pseudo encoding is not handled by the
				   enabled extensions, search through all
				   extensions. */
				rfbProtocolExtension* e;

				for(e = rfbGetExtensionIterator(); e;) {
					int* encs = e->pseudoEncodings;
					while(encs && *encs!=0) {
						if(*encs==(int)enc) {
							void* data = NULL;
							if(!e->enablePseudoEncoding(cl, &data, (int)enc)) {
								rfbLog("Installed extension pretends to handle pseudo encoding 0x%x, but does not!\n",(int)enc);
							} else {
								rfbEnableExtension(cl, e, data);
								handled = TRUE;
								e = NULL;
								break;
							}
						}
						encs++;
					}

					if(e)
						e = e->next;
				}
				rfbReleaseExtensionIterator();

				if(!handled)
					rfbLog("rfbProcessClientNormalMessage: "
					    "ignoring unsupported encoding type %s\n",
					    encodingName(enc,encBuf,sizeof(encBuf)));
			}
		}
            }
        }



        if (cl->preferredEncoding == -1) {
            if (lastPreferredEncoding==-1) {
                cl->preferredEncoding = rfbEncodingRaw;
                rfbLog("Defaulting to %s encoding for client %s\n", encodingName(cl->preferredEncoding,encBuf,sizeof(encBuf)),cl->host);
            }
            else {
                cl->preferredEncoding = lastPreferredEncoding;
                rfbLog("Sticking with %s encoding for client %s\n", encodingName(cl->preferredEncoding,encBuf,sizeof(encBuf)),cl->host);
            }
        }
        else
        {
          if (lastPreferredEncoding==-1) {
              rfbLog("Using %s encoding for client %s\n", encodingName(cl->preferredEncoding,encBuf,sizeof(encBuf)),cl->host);
          } else {
              rfbLog("Switching from %s to %s Encoding for client %s\n", 
                  encodingName(lastPreferredEncoding,encBuf2,sizeof(encBuf2)),
                  encodingName(cl->preferredEncoding,encBuf,sizeof(encBuf)), cl->host);
          }
        }
        
	if (cl->enableCursorPosUpdates && !cl->enableCursorShapeUpdates) {
	  rfbLog("Disabling cursor position updates for client %s\n",
		 cl->host);
	  cl->enableCursorPosUpdates = FALSE;
	}

        return;
    }


    case rfbFramebufferUpdateRequest:
    {
        sraRegionPtr tmpRegion;

        if ((n = rfbReadExact(cl, ((char *)&msg) + 1,
                           sz_rfbFramebufferUpdateRequestMsg-1)) <= 0) {
            if (n != 0)
                rfbLogPerror("rfbProcessClientNormalMessage: read");
            rfbCloseClient(cl);
            return;
        }

        rfbStatRecordMessageRcvd(cl, msg.type, sz_rfbFramebufferUpdateRequestMsg,sz_rfbFramebufferUpdateRequestMsg);

        /* The values come in based on the scaled screen, we need to convert them to
         * values based on the main screen's coordinate system
         */
	if(!rectSwapIfLEAndClip(&msg.fur.x,&msg.fur.y,&msg.fur.w,&msg.fur.h,cl))
	{
	        rfbLog("Warning, ignoring rfbFramebufferUpdateRequest: %dXx%dY-%dWx%dH\n",msg.fur.x, msg.fur.y, msg.fur.w, msg.fur.h);
		return;
        }
 
        
	tmpRegion =
	  sraRgnCreateRect(msg.fur.x,
			   msg.fur.y,
			   msg.fur.x+msg.fur.w,
			   msg.fur.y+msg.fur.h);

        LOCK(cl->updateMutex);
	sraRgnOr(cl->requestedRegion,tmpRegion);

	if (!cl->readyForSetColourMapEntries) {
	    /* client hasn't sent a SetPixelFormat so is using server's */
	    cl->readyForSetColourMapEntries = TRUE;
	    if (!cl->format.trueColour) {
		if (!rfbSetClientColourMap(cl, 0, 0)) {
		    sraRgnDestroy(tmpRegion);
		    TSIGNAL(cl->updateCond);
		    UNLOCK(cl->updateMutex);
		    return;
		}
	    }
	}

       if (!msg.fur.incremental) {
	    sraRgnOr(cl->modifiedRegion,tmpRegion);
	    sraRgnSubtract(cl->copyRegion,tmpRegion);
       }
       TSIGNAL(cl->updateCond);
       UNLOCK(cl->updateMutex);

       sraRgnDestroy(tmpRegion);

       return;
    }

    case rfbKeyEvent:

	if ((n = rfbReadExact(cl, ((char *)&msg) + 1,
			   sz_rfbKeyEventMsg - 1)) <= 0) {
	    if (n != 0)
		rfbLogPerror("rfbProcessClientNormalMessage: read");
	    rfbCloseClient(cl);
	    return;
	}

	rfbStatRecordMessageRcvd(cl, msg.type, sz_rfbKeyEventMsg, sz_rfbKeyEventMsg);

	if(!cl->viewOnly) {
	    cl->screen->kbdAddEvent(msg.ke.down, (rfbKeySym)Swap32IfLE(msg.ke.key), cl);
	}

        return;


    case rfbPointerEvent:

	if ((n = rfbReadExact(cl, ((char *)&msg) + 1,
			   sz_rfbPointerEventMsg - 1)) <= 0) {
	    if (n != 0)
		rfbLogPerror("rfbProcessClientNormalMessage: read");
	    rfbCloseClient(cl);
	    return;
	}

	rfbStatRecordMessageRcvd(cl, msg.type, sz_rfbPointerEventMsg, sz_rfbPointerEventMsg);
	
	if (cl->screen->pointerClient && cl->screen->pointerClient != cl)
	    return;

	if (msg.pe.buttonMask == 0)
	    cl->screen->pointerClient = NULL;
	else
	    cl->screen->pointerClient = cl;

	if(!cl->viewOnly) {
	    if (msg.pe.buttonMask != cl->lastPtrButtons ||
		    cl->screen->deferPtrUpdateTime == 0) {
		cl->screen->ptrAddEvent(msg.pe.buttonMask,
			ScaleX(cl->scaledScreen, cl->screen, Swap16IfLE(msg.pe.x)), 
			ScaleY(cl->scaledScreen, cl->screen, Swap16IfLE(msg.pe.y)),
			cl);
		cl->lastPtrButtons = msg.pe.buttonMask;
	    } else {
		cl->lastPtrX = ScaleX(cl->scaledScreen, cl->screen, Swap16IfLE(msg.pe.x));
		cl->lastPtrY = ScaleY(cl->scaledScreen, cl->screen, Swap16IfLE(msg.pe.y));
		cl->lastPtrButtons = msg.pe.buttonMask;
	    }
      }      
      return;


    case rfbFileTransfer:
        if ((n = rfbReadExact(cl, ((char *)&msg) + 1,
                              sz_rfbFileTransferMsg - 1)) <= 0) {
            if (n != 0)
                rfbLogPerror("rfbProcessClientNormalMessage: read");
            rfbCloseClient(cl);
            return;
        }
        msg.ft.size         = Swap32IfLE(msg.ft.size);
        msg.ft.length       = Swap32IfLE(msg.ft.length);
        /* record statistics in rfbProcessFileTransfer as length is filled with garbage when it is not valid */
        rfbProcessFileTransfer(cl, msg.ft.contentType, msg.ft.contentParam, msg.ft.size, msg.ft.length);
        return;

    case rfbSetSW:
        if ((n = rfbReadExact(cl, ((char *)&msg) + 1,
                              sz_rfbSetSWMsg - 1)) <= 0) {
            if (n != 0)
                rfbLogPerror("rfbProcessClientNormalMessage: read");
            rfbCloseClient(cl);
            return;
        }
        msg.sw.x = Swap16IfLE(msg.sw.x);
        msg.sw.y = Swap16IfLE(msg.sw.y);
        rfbStatRecordMessageRcvd(cl, msg.type, sz_rfbSetSWMsg, sz_rfbSetSWMsg);
        /* msg.sw.status is not initialized in the ultraVNC viewer and contains random numbers (why???) */

        rfbLog("Received a rfbSetSingleWindow(%d x, %d y)\n", msg.sw.x, msg.sw.y);
        if (cl->screen->setSingleWindow!=NULL)
            cl->screen->setSingleWindow(cl, msg.sw.x, msg.sw.y);
        return;

    case rfbSetServerInput:
        if ((n = rfbReadExact(cl, ((char *)&msg) + 1,
                              sz_rfbSetServerInputMsg - 1)) <= 0) {
            if (n != 0)
                rfbLogPerror("rfbProcessClientNormalMessage: read");
            rfbCloseClient(cl);
            return;
        }
        rfbStatRecordMessageRcvd(cl, msg.type, sz_rfbSetServerInputMsg, sz_rfbSetServerInputMsg);

        /* msg.sim.pad is not initialized in the ultraVNC viewer and contains random numbers (why???) */
        /* msg.sim.pad = Swap16IfLE(msg.sim.pad); */

        rfbLog("Received a rfbSetServerInput(%d status)\n", msg.sim.status);
        if (cl->screen->setServerInput!=NULL)
            cl->screen->setServerInput(cl, msg.sim.status);
        return;
        
    case rfbTextChat:
        if ((n = rfbReadExact(cl, ((char *)&msg) + 1,
                              sz_rfbTextChatMsg - 1)) <= 0) {
            if (n != 0)
                rfbLogPerror("rfbProcessClientNormalMessage: read");
            rfbCloseClient(cl);
            return;
        }
        
        msg.tc.pad2   = Swap16IfLE(msg.tc.pad2);
        msg.tc.length = Swap32IfLE(msg.tc.length);

        switch (msg.tc.length) {
        case rfbTextChatOpen:
        case rfbTextChatClose:
        case rfbTextChatFinished:
            /* commands do not have text following */
            /* Why couldn't they have used the pad byte??? */
            str=NULL;
            rfbStatRecordMessageRcvd(cl, msg.type, sz_rfbTextChatMsg, sz_rfbTextChatMsg);
            break;
        default:
            if ((msg.tc.length>0) && (msg.tc.length<rfbTextMaxSize))
            {
                str = (char *)malloc(msg.tc.length);
                if (str==NULL)
                {
                    rfbLog("Unable to malloc %d bytes for a TextChat Message\n", msg.tc.length);
                    rfbCloseClient(cl);
                    return;
                }
                if ((n = rfbReadExact(cl, str, msg.tc.length)) <= 0) {
                    if (n != 0)
                        rfbLogPerror("rfbProcessClientNormalMessage: read");
                    free(str);
                    rfbCloseClient(cl);
                    return;
                }
                rfbStatRecordMessageRcvd(cl, msg.type, sz_rfbTextChatMsg+msg.tc.length, sz_rfbTextChatMsg+msg.tc.length);
            }
            else
            {
                /* This should never happen */
                rfbLog("client sent us a Text Message that is too big %d>%d\n", msg.tc.length, rfbTextMaxSize);
                rfbCloseClient(cl);
                return;
            }
        }

        /* Note: length can be commands: rfbTextChatOpen, rfbTextChatClose, and rfbTextChatFinished
         * at which point, the str is NULL (as it is not sent)
         */
        if (cl->screen->setTextChat!=NULL)
            cl->screen->setTextChat(cl, msg.tc.length, str);

        free(str);
        return;


    case rfbClientCutText:

	if ((n = rfbReadExact(cl, ((char *)&msg) + 1,
			   sz_rfbClientCutTextMsg - 1)) <= 0) {
	    if (n != 0)
		rfbLogPerror("rfbProcessClientNormalMessage: read");
	    rfbCloseClient(cl);
	    return;
	}

	msg.cct.length = Swap32IfLE(msg.cct.length);

	str = (char *)malloc(msg.cct.length);
	if (str == NULL) {
		rfbLogPerror("rfbProcessClientNormalMessage: not enough memory");
		rfbCloseClient(cl);
		return;
	}

	if ((n = rfbReadExact(cl, str, msg.cct.length)) <= 0) {
	    if (n != 0)
	        rfbLogPerror("rfbProcessClientNormalMessage: read");
	    free(str);
	    rfbCloseClient(cl);
	    return;
	}
	rfbStatRecordMessageRcvd(cl, msg.type, sz_rfbClientCutTextMsg+msg.cct.length, sz_rfbClientCutTextMsg+msg.cct.length);
	if(!cl->viewOnly) {
	    cl->screen->setXCutText(str, msg.cct.length, cl);
	}
	free(str);

        return;

    case rfbPalmVNCSetScaleFactor:
      cl->PalmVNC = TRUE;
      if ((n = rfbReadExact(cl, ((char *)&msg) + 1,
          sz_rfbSetScaleMsg - 1)) <= 0) {
          if (n != 0)
            rfbLogPerror("rfbProcessClientNormalMessage: read");
          rfbCloseClient(cl);
          return;
      }
      rfbStatRecordMessageRcvd(cl, msg.type, sz_rfbSetScaleMsg, sz_rfbSetScaleMsg);
      rfbLog("rfbSetScale(%d)\n", msg.ssc.scale);
      rfbScalingSetup(cl,cl->screen->width/msg.ssc.scale, cl->screen->height/msg.ssc.scale);

      rfbSendNewScaleSize(cl);
      return;
      
    case rfbSetScale:

      if ((n = rfbReadExact(cl, ((char *)&msg) + 1,
          sz_rfbSetScaleMsg - 1)) <= 0) {
          if (n != 0)
            rfbLogPerror("rfbProcessClientNormalMessage: read");
          rfbCloseClient(cl);
          return;
      }
      rfbStatRecordMessageRcvd(cl, msg.type, sz_rfbSetScaleMsg, sz_rfbSetScaleMsg);
      rfbLog("rfbSetScale(%d)\n", msg.ssc.scale);
      rfbScalingSetup(cl,cl->screen->width/msg.ssc.scale, cl->screen->height/msg.ssc.scale);

      rfbSendNewScaleSize(cl);
      return;

    case rfbXvp:

      if ((n = rfbReadExact(cl, ((char *)&msg) + 1,
          sz_rfbXvpMsg - 1)) <= 0) {
          if (n != 0)
            rfbLogPerror("rfbProcessClientNormalMessage: read");
          rfbCloseClient(cl);
          return;
      }
      rfbStatRecordMessageRcvd(cl, msg.type, sz_rfbXvpMsg, sz_rfbXvpMsg);

      /* only version when is defined, so echo back a fail */
      if(msg.xvp.version != 1) {
	rfbSendXvp(cl, msg.xvp.version, rfbXvp_Fail);
      }
      else {
	/* if the hook exists and fails, send a fail msg */
	if(cl->screen->xvpHook && !cl->screen->xvpHook(cl, msg.xvp.version, msg.xvp.code))
	  rfbSendXvp(cl, 1, rfbXvp_Fail);
      }
      return;

    default:
	{
	    rfbExtensionData *e,*next;

	    for(e=cl->extensions; e;) {
		next = e->next;
		if(e->extension->handleMessage &&
			e->extension->handleMessage(cl, e->data, &msg))
                {
                    rfbStatRecordMessageRcvd(cl, msg.type, 0, 0); /* Extension should handle this */
		    return;
                }
		e = next;
	    }

	    rfbLog("rfbProcessClientNormalMessage: unknown message type %d\n",
		    msg.type);
	    rfbLog(" ... closing connection\n");
	    rfbCloseClient(cl);
	    return;
	}
    }
}