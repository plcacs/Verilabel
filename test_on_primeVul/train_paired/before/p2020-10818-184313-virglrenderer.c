void vrend_renderer_context_create_internal(uint32_t handle, uint32_t nlen,
                                            const char *debug_name)
{
   struct vrend_decode_ctx *dctx;

   if (handle >= VREND_MAX_CTX)
      return;

   dctx = malloc(sizeof(struct vrend_decode_ctx));
   if (!dctx)
      return;

   dctx->grctx = vrend_create_context(handle, nlen, debug_name);
   if (!dctx->grctx) {
      free(dctx);
      return;
   }

   dctx->ds = &dctx->ids;

   dec_ctx[handle] = dctx;
}