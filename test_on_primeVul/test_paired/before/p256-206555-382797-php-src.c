static int dynamicGetbuf (gdIOCtxPtr ctx, void *buf, int len)
{
	int rlen, remain;
	dpIOCtxPtr dctx;
	dynamicPtr *dp;

	dctx = (dpIOCtxPtr) ctx;
	dp = dctx->dp;

	remain = dp->logicalSize - dp->pos;
	if (remain >= len) {
		rlen = len;
	} else {
		if (remain == 0) {
			return EOF;
		}
		rlen = remain;
	}

	memcpy(buf, (void *) ((char *) dp->data + dp->pos), rlen);
	dp->pos += rlen;

	return rlen;
}