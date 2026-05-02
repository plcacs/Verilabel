cnt_recv_prep(struct req *req, const char *ci)
{
	const char *xff;

	if (req->restarts == 0) {
		/*
		 * This really should be done earlier, but we want to capture
		 * it in the VSL log.
		 */
		http_CollectHdr(req->http, H_X_Forwarded_For);
		if (http_GetHdr(req->http, H_X_Forwarded_For, &xff)) {
			http_Unset(req->http, H_X_Forwarded_For);
			http_PrintfHeader(req->http, "X-Forwarded-For: %s, %s",
			    xff, ci);
		} else {
			http_PrintfHeader(req->http, "X-Forwarded-For: %s", ci);
		}
		http_CollectHdr(req->http, H_Cache_Control);

		/* By default we use the first backend */
		req->director_hint = VCL_DefaultDirector(req->vcl);

		req->d_ttl = -1;
		req->d_grace = -1;
		req->disable_esi = 0;
		req->hash_always_miss = 0;
		req->hash_ignore_busy = 0;
		req->client_identity = NULL;
		req->storage = NULL;
	}

	req->vdc->retval = 0;
	req->is_hit = 0;
	req->is_hitmiss = 0;
	req->is_hitpass = 0;
}