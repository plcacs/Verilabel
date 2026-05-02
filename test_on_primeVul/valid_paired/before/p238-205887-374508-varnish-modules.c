vmod_append(VRT_CTX, VCL_HEADER hdr, VCL_STRANDS s)
{
	struct http *hp;
	struct strands st[1];
	const char *p[s->n + 2];
	const char *b;

	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);

	/* prefix the strand with $hdr_name + space */
	p[0] = hdr->what + 1;
	p[1] = " ";
	AN(memcpy(p + 2, s->p, s->n * sizeof *s->p));
	st->n = s->n + 2;
	st->p = p;

	b = VRT_StrandsWS(ctx->ws, NULL, st);

	hp = VRT_selecthttp(ctx, hdr->where);
	http_SetHeader(hp, b);
}