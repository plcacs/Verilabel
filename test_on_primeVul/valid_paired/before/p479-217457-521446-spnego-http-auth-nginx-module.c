ngx_http_auth_spnego_handler(
        ngx_http_request_t * r)
{
    ngx_int_t ret = NGX_DECLINED;
    ngx_http_auth_spnego_ctx_t *ctx;
    ngx_http_auth_spnego_loc_conf_t *alcf;

    alcf = ngx_http_get_module_loc_conf(r, ngx_http_auth_spnego_module);

    if (alcf->protect == 0) {
        return NGX_DECLINED;
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_auth_spnego_module);
    if (NULL == ctx) {
        ctx = ngx_palloc(r->pool, sizeof(ngx_http_auth_spnego_ctx_t));
        if (NULL == ctx) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        ctx->token.len = 0;
        ctx->token.data = NULL;
        ctx->head = 0;
        ctx->ret = NGX_HTTP_UNAUTHORIZED;
        ngx_http_set_ctx(r, ctx, ngx_http_auth_spnego_module);
    }

    spnego_debug3("SSO auth handling IN: token.len=%d, head=%d, ret=%d",
            ctx->token.len, ctx->head, ctx->ret);

    if (ctx->token.len && ctx->head) {
        spnego_debug1("Found token and head, returning %d", ctx->ret);
        return ctx->ret;
    }

    if (NULL != r->headers_in.user.data) {
        spnego_debug0("User header set");
        return NGX_OK;
    }

    spnego_debug0("Begin auth");

    if (alcf->allow_basic) {
        spnego_debug0("Detect basic auth");
        ret = ngx_http_auth_basic_user(r);
        if (NGX_OK == ret) {
            spnego_debug0("Basic auth credentials supplied by client");
            /* If basic auth is enabled and basic creds are supplied
             * attempt basic auth.  If we attempt basic auth, we do
             * not fall through to real SPNEGO */
            if (NGX_DECLINED == ngx_http_auth_spnego_basic(r, ctx, alcf)) {
                spnego_debug0("Basic auth failed");
                if (NGX_ERROR == ngx_http_auth_spnego_headers_basic_only(r, ctx, alcf)) {
                    spnego_debug0("Error setting headers");
                    return (ctx->ret = NGX_HTTP_INTERNAL_SERVER_ERROR);
                }
                return (ctx->ret = NGX_HTTP_UNAUTHORIZED);
            }

            if (!ngx_spnego_authorized_principal(r, &r->headers_in.user, alcf)) {
                spnego_debug0("User not authorized");
                return (ctx->ret = NGX_HTTP_FORBIDDEN);
            }

            spnego_debug0("Basic auth succeeded");
            return (ctx->ret = NGX_OK);
        }
    }

    /* Basic auth either disabled or not supplied by client */
    spnego_debug0("Detect SPNEGO token");
    ret = ngx_http_auth_spnego_token(r, ctx);
    if (NGX_OK == ret) {
        spnego_debug0("Client sent a reasonable Negotiate header");
        ret = ngx_http_auth_spnego_auth_user_gss(r, ctx, alcf);
        if (NGX_ERROR == ret) {
            spnego_debug0("GSSAPI failed");
            return (ctx->ret = NGX_HTTP_INTERNAL_SERVER_ERROR);
        }
        /* There are chances that client knows about Negotiate
         * but doesn't support GSSAPI. We could attempt to fall
         * back to basic here... */
        if (NGX_DECLINED == ret) {
            spnego_debug0("GSSAPI failed");
            if(!alcf->allow_basic) {
                return (ctx->ret = NGX_HTTP_FORBIDDEN);
            }
            if (NGX_ERROR == ngx_http_auth_spnego_headers_basic_only(r, ctx, alcf)) {
                spnego_debug0("Error setting headers");
                return (ctx->ret = NGX_HTTP_INTERNAL_SERVER_ERROR);
            }
            return (ctx->ret = NGX_HTTP_UNAUTHORIZED);
        }

        if (!ngx_spnego_authorized_principal(r, &r->headers_in.user, alcf)) {
            spnego_debug0("User not authorized");
            return (ctx->ret = NGX_HTTP_FORBIDDEN);
        }

        spnego_debug0("GSSAPI auth succeeded");
    }

    ngx_str_t *token_out_b64 = NULL;
    switch(ret) {
        case NGX_DECLINED: /* DECLINED, but not yet FORBIDDEN */
            ctx->ret = NGX_HTTP_UNAUTHORIZED;
            break;
        case NGX_OK:
            ctx->ret = NGX_OK;
            token_out_b64 = &ctx->token_out_b64;
            break;
        case NGX_ERROR:
        default:
            ctx->ret = NGX_HTTP_INTERNAL_SERVER_ERROR;
            break;
    }

    if (NGX_ERROR == ngx_http_auth_spnego_headers(r, ctx, token_out_b64, alcf)) {
        spnego_debug0("Error setting headers");
        ctx->ret = NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    spnego_debug3("SSO auth handling OUT: token.len=%d, head=%d, ret=%d",
            ctx->token.len, ctx->head, ctx->ret);
    return ctx->ret;
}