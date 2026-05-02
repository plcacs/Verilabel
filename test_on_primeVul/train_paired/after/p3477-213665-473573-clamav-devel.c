int32_t cli_bcapi_extract_new(struct cli_bc_ctx *ctx, int32_t id)
{
    cli_ctx *cctx;
    int res = -1;

    cli_event_count(EV, BCEV_EXTRACTED);
    cli_dbgmsg("previous tempfile had %u bytes\n", ctx->written);
    if (!ctx->written)
	return 0;
    if (ctx->ctx && cli_updatelimits(ctx->ctx, ctx->written))
	return -1;
    ctx->written = 0;
    lseek(ctx->outfd, 0, SEEK_SET);
    cli_dbgmsg("bytecode: scanning extracted file %s\n", ctx->tempfile);
    cctx = (cli_ctx*)ctx->ctx;
    if (cctx) {
	cli_file_t current = cctx->container_type;
	if (ctx->containertype != CL_TYPE_ANY)
	    cctx->container_type = ctx->containertype;
	cctx->recursion++;
	res = cli_magic_scandesc(ctx->outfd, cctx);
	cctx->recursion--;
	cctx->container_type = current;
	if (res == CL_VIRUS) {
	    if (cctx->virname)
		ctx->virname = *cctx->virname;
	    ctx->found = 1;
	}
    }
    if ((cctx && cctx->engine->keeptmp) ||
	(ftruncate(ctx->outfd, 0) == -1)) {

	close(ctx->outfd);
	if (!(cctx && cctx->engine->keeptmp) && ctx->tempfile)
	    cli_unlink(ctx->tempfile);
	free(ctx->tempfile);
	ctx->tempfile = NULL;
	ctx->outfd = 0;
    }
    cli_dbgmsg("bytecode: extracting new file with id %u\n", id);
    return res;
}