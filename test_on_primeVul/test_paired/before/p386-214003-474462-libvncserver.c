HandleCoRREBPP (rfbClient* client, int rx, int ry, int rw, int rh)
{
    rfbRREHeader hdr;
    int i;
    CARDBPP pix;
    uint8_t *ptr;
    int x, y, w, h;

    if (!ReadFromRFBServer(client, (char *)&hdr, sz_rfbRREHeader))
	return FALSE;

    hdr.nSubrects = rfbClientSwap32IfLE(hdr.nSubrects);

    if (!ReadFromRFBServer(client, (char *)&pix, sizeof(pix)))
	return FALSE;

    client->GotFillRect(client, rx, ry, rw, rh, pix);

    if (hdr.nSubrects * (4 + (BPP / 8)) > RFB_BUFFER_SIZE || !ReadFromRFBServer(client, client->buffer, hdr.nSubrects * (4 + (BPP / 8))))
	return FALSE;

    ptr = (uint8_t *)client->buffer;

    for (i = 0; i < hdr.nSubrects; i++) {
	pix = *(CARDBPP *)ptr;
	ptr += BPP/8;
	x = *ptr++;
	y = *ptr++;
	w = *ptr++;
	h = *ptr++;

	client->GotFillRect(client, rx+x, ry+y, w, h, pix);
    }

    return TRUE;
}