static void wait_for_child_to_die(void *ctx)
{
	REQUEST *request = ctx;

	rad_assert(request->magic == REQUEST_MAGIC);
	remove_from_request_hash(request);

	/*
	 *	If it's still queued (waiting for a thread to pick it
	 *	up) OR, it's running AND there's still a child thread
	 *	handling it, THEN delay some more.
	 */
	if ((request->child_state == REQUEST_QUEUED) ||
	    ((request->child_state == REQUEST_RUNNING) &&
	     (pthread_equal(request->child_pid, NO_SUCH_CHILD_PID) == 0))) {

		/*
		 *	Cap delay at max_request_time
		 */
		if (request->delay < (USEC * request->root->max_request_time)) {
			request->delay += (request->delay >> 1);
			radlog(L_INFO, "WARNING: Child is hung for request %u in component %s module %s.",
			       request->number, request->component, request->module);
		} else {
			request->delay = USEC * request->root->max_request_time;
			RDEBUG2("WARNING: Child is still stuck for request %u",
				request->number);
		}
		tv_add(&request->when, request->delay);

		INSERT_EVENT(wait_for_child_to_die, request);
		return;
	}

	RDEBUG2("Child is finally responsive for request %u", request->number);

#ifdef WITH_PROXY
	if (request->proxy) {
		wait_for_proxy_id_to_expire(request);
		return;
	}
#endif

	ev_request_free(&request);
}