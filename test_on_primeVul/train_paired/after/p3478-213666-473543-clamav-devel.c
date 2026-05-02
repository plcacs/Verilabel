int cli_bytecode_runhook(cli_ctx *cctx, const struct cl_engine *engine, struct cli_bc_ctx *ctx,
			 unsigned id, fmap_t *map, const char **virname)
{
    const unsigned *hooks = engine->hooks[id - _BC_START_HOOKS];
    unsigned i, hooks_cnt = engine->hooks_cnt[id - _BC_START_HOOKS];
    int ret;
    unsigned executed = 0, breakflag = 0, errorflag = 0;

    cli_dbgmsg("Bytecode executing hook id %u (%u hooks)\n", id, hooks_cnt);
    /* restore match counts */
    cli_bytecode_context_setfile(ctx, map);
    ctx->hooks.match_counts = ctx->lsigcnt;
    ctx->hooks.match_offsets = ctx->lsigoff;
    for (i=0;i < hooks_cnt;i++) {
	const struct cli_bc *bc = &engine->bcs.all_bcs[hooks[i]];
	if (bc->lsig) {
	    if (!cctx->hook_lsig_matches ||
		!cli_bitset_test(cctx->hook_lsig_matches, bc->hook_lsig_id-1))
		continue;
	    cli_dbgmsg("Bytecode: executing bytecode %u (lsig matched)\n" , bc->id);
	}
	cli_bytecode_context_setfuncid(ctx, bc, 0);
	ret = cli_bytecode_run(&engine->bcs, bc, ctx);
	executed++;
	if (ret != CL_SUCCESS) {
	    cli_warnmsg("Bytecode %u failed to run: %s\n", bc->id, cl_strerror(ret));
	    errorflag = 1;
	    continue;
	}
	if (ctx->virname) {
	    cli_dbgmsg("Bytecode found virus: %s\n", ctx->virname);
	    if (virname)
		*virname = ctx->virname;
	    cli_bytecode_context_clear(ctx);
	    return CL_VIRUS;
	}
	ret = cli_bytecode_context_getresult_int(ctx);
	/* TODO: use prefix here */
	cli_dbgmsg("Bytecode %u returned %u\n", bc->id, ret);
	if (ret == 0xcea5e) {
	    cli_dbgmsg("Bytecode set BREAK flag in hook!\n");
	    breakflag = 1;
	}
	if (!ret) {
	    char *tempfile;
	    int fd = cli_bytecode_context_getresult_file(ctx, &tempfile);
	    if (fd && fd != -1) {
		if (cctx && cctx->engine->keeptmp)
		    cli_dbgmsg("Bytecode %u unpacked file saved in %s\n",
			       bc->id, tempfile);
		else
		    cli_dbgmsg("Bytecode %u unpacked file\n", bc->id);
		lseek(fd, 0, SEEK_SET);
		cli_dbgmsg("***** Scanning unpacked file ******\n");
		cctx->recursion++;
		ret = cli_magic_scandesc(fd, cctx);
		cctx->recursion--;
		if (!cctx || !cctx->engine->keeptmp)
		    if (ftruncate(fd, 0) == -1)
			cli_dbgmsg("ftruncate failed on %d\n", fd);
		close(fd);
		if (!cctx || !cctx->engine->keeptmp) {
		    if (tempfile && cli_unlink(tempfile))
			ret = CL_EUNLINK;
		}
		free(tempfile);
		if (ret != CL_CLEAN) {
		    if (ret == CL_VIRUS)
			cli_dbgmsg("Scanning unpacked file by bytecode %u found a virus\n", bc->id);
		    cli_bytecode_context_clear(ctx);
		    return ret;
		}
		cli_bytecode_context_reset(ctx);
		continue;
	    }
	}
	cli_bytecode_context_reset(ctx);
    }
    if (executed)
	cli_dbgmsg("Bytecode: executed %u bytecodes for this hook\n", executed);
    else
	cli_dbgmsg("Bytecode: no logical signature matched, no bytecode executed\n");
    if (errorflag && cctx && cctx->engine->bytecode_mode == CL_BYTECODE_MODE_TEST)
	return CL_EBYTECODE_TESTFAIL;
    return breakflag ? CL_BREAK : CL_CLEAN;
}