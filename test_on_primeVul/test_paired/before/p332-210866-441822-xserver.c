SProcXkbSelectEvents(ClientPtr client)
{
    REQUEST(xkbSelectEventsReq);

    swaps(&stuff->length);
    REQUEST_AT_LEAST_SIZE(xkbSelectEventsReq);
    swaps(&stuff->deviceSpec);
    swaps(&stuff->affectWhich);
    swaps(&stuff->clear);
    swaps(&stuff->selectAll);
    swaps(&stuff->affectMap);
    swaps(&stuff->map);
    if ((stuff->affectWhich & (~XkbMapNotifyMask)) != 0) {
        union {
            BOOL *b;
            CARD8 *c8;
            CARD16 *c16;
            CARD32 *c32;
        } from;
        register unsigned bit, ndx, maskLeft, dataLeft, size;

        from.c8 = (CARD8 *) &stuff[1];
        dataLeft = (stuff->length * 4) - SIZEOF(xkbSelectEventsReq);
        maskLeft = (stuff->affectWhich & (~XkbMapNotifyMask));
        for (ndx = 0, bit = 1; (maskLeft != 0); ndx++, bit <<= 1) {
            if (((bit & maskLeft) == 0) || (ndx == XkbMapNotify))
                continue;
            maskLeft &= ~bit;
            if ((stuff->selectAll & bit) || (stuff->clear & bit))
                continue;
            switch (ndx) {
            case XkbNewKeyboardNotify:
            case XkbStateNotify:
            case XkbNamesNotify:
            case XkbAccessXNotify:
            case XkbExtensionDeviceNotify:
                size = 2;
                break;
            case XkbControlsNotify:
            case XkbIndicatorStateNotify:
            case XkbIndicatorMapNotify:
                size = 4;
                break;
            case XkbBellNotify:
            case XkbActionMessage:
            case XkbCompatMapNotify:
                size = 1;
                break;
            default:
                client->errorValue = _XkbErrCode2(0x1, bit);
                return BadValue;
            }
            if (dataLeft < (size * 2))
                return BadLength;
            if (size == 2) {
                swaps(&from.c16[0]);
                swaps(&from.c16[1]);
            }
            else if (size == 4) {
                swapl(&from.c32[0]);
                swapl(&from.c32[1]);
            }
            else {
                size = 2;
            }
            from.c8 += (size * 2);
            dataLeft -= (size * 2);
        }
        if (dataLeft > 2) {
            ErrorF("[xkb] Extra data (%d bytes) after SelectEvents\n",
                   dataLeft);
            return BadLength;
        }
    }
    return ProcXkbSelectEvents(client);
}