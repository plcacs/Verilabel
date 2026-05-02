bool initiate_stratum(struct pool *pool)
{
	char s[RBUFSIZE], *sret = NULL, *nonce1, *sessionid;
	json_t *val = NULL, *res_val, *err_val;
	bool ret = false, recvd = false;
	json_error_t err;
	int n2size;

	if (!setup_stratum_curl(pool))
		goto out;

resend:
	if (pool->sessionid)
		sprintf(s, "{\"id\": %d, \"method\": \"mining.subscribe\", \"params\": [\"%s\"]}", swork_id++, pool->sessionid);
	else
		sprintf(s, "{\"id\": %d, \"method\": \"mining.subscribe\", \"params\": []}", swork_id++);

	if (!__stratum_send(pool, s, strlen(s))) {
		applog(LOG_DEBUG, "Failed to send s in initiate_stratum");
		goto out;
	}

	if (!socket_full(pool, true)) {
		applog(LOG_DEBUG, "Timed out waiting for response in initiate_stratum");
		goto out;
	}

	sret = recv_line(pool);
	if (!sret)
		goto out;

	recvd = true;

	val = JSON_LOADS(sret, &err);
	free(sret);
	if (!val) {
		applog(LOG_INFO, "JSON decode failed(%d): %s", err.line, err.text);
		goto out;
	}

	res_val = json_object_get(val, "result");
	err_val = json_object_get(val, "error");

	if (!res_val || json_is_null(res_val) ||
	    (err_val && !json_is_null(err_val))) {
		char *ss;

		if (err_val)
			ss = json_dumps(err_val, JSON_INDENT(3));
		else
			ss = strdup("(unknown reason)");

		applog(LOG_INFO, "JSON-RPC decode failed: %s", ss);

		free(ss);

		goto out;
	}

	sessionid = json_array_string(json_array_get(res_val, 0), 1);
	if (!sessionid) {
		applog(LOG_INFO, "Failed to get sessionid in initiate_stratum");
		goto out;
	}
	nonce1 = json_array_string(res_val, 1);
	if (!nonce1) {
		applog(LOG_INFO, "Failed to get nonce1 in initiate_stratum");
		free(sessionid);
		goto out;
	}
	n2size = json_integer_value(json_array_get(res_val, 2));
	if (!n2size) {
		applog(LOG_INFO, "Failed to get n2size in initiate_stratum");
		free(sessionid);
		free(nonce1);
		goto out;
	}

	mutex_lock(&pool->pool_lock);
	pool->sessionid = sessionid;
	free(pool->nonce1);
	pool->nonce1 = nonce1;
	pool->n1_len = strlen(nonce1) / 2;
	pool->n2size = n2size;
	mutex_unlock(&pool->pool_lock);

	applog(LOG_DEBUG, "Pool %d stratum session id: %s", pool->pool_no, pool->sessionid);

	ret = true;
out:
	if (val)
		json_decref(val);

	if (ret) {
		if (!pool->stratum_url)
			pool->stratum_url = pool->sockaddr_url;
		pool->stratum_active = true;
		pool->swork.diff = 1;
		if (opt_protocol) {
			applog(LOG_DEBUG, "Pool %d confirmed mining.subscribe with extranonce1 %s extran2size %d",
			       pool->pool_no, pool->nonce1, pool->n2size);
		}
	} else {
		if (recvd && pool->sessionid) {
			/* Reset the sessionid used for stratum resuming in case the pool
			* does not support it, or does not know how to respond to the
			* presence of the sessionid parameter. */
			mutex_lock(&pool->pool_lock);
			free(pool->sessionid);
			free(pool->nonce1);
			pool->sessionid = pool->nonce1 = NULL;
			mutex_unlock(&pool->pool_lock);
			applog(LOG_DEBUG, "Failed to resume stratum, trying afresh");
			goto resend;
		}
		applog(LOG_DEBUG, "Initiate stratum failed");
		if (pool->sock != INVSOCK) {
			shutdown(pool->sock, SHUT_RDWR);
			pool->sock = INVSOCK;
		}
	}

	return ret;
}