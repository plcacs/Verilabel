static void *gp_worker_main(void *pvt)
{
    struct gp_thread *t = (struct gp_thread *)pvt;
    struct gp_query *q = NULL;
    char dummy = 0;
    int ret;

    while (!t->pool->shutdown) {

        /* initialize debug client id to 0 until work is scheduled */
        gp_debug_set_conn_id(0);

        /* ======> COND_MUTEX */
        pthread_mutex_lock(&t->cond_mutex);
        while (t->query == NULL) {
            /* wait for next query */
            pthread_cond_wait(&t->cond_wakeup, &t->cond_mutex);
            if (t->pool->shutdown) {
                pthread_exit(NULL);
            }
        }

        /* grab the query off the shared pointer */
        q = t->query;
        t->query = NULL;

        /* <====== COND_MUTEX */
        pthread_mutex_unlock(&t->cond_mutex);

        /* set client id before hndling requests */
        gp_debug_set_conn_id(gp_conn_get_cid(q->conn));

        /* handle the client request */
        GPDEBUGN(3, "[status] Handling query input: %p (%zu)\n", q->buffer,
                 q->buflen);
        gp_handle_query(t->pool, q);
        GPDEBUGN(3 ,"[status] Handling query output: %p (%zu)\n", q->buffer,
                 q->buflen);

        /* now get lock on main queue, to play with the reply list */
        /* ======> POOL LOCK */
        pthread_mutex_lock(&t->pool->lock);

        /* put back query so that dispatcher can send reply */
        q->next = t->pool->reply_list;
        t->pool->reply_list = q;

        /* add us back to the free list but only if we are not
         * shutting down */
        if (!t->pool->shutdown) {
            LIST_DEL(t->pool->busy_list, t);
            LIST_ADD(t->pool->free_list, t);
        }

        /* <====== POOL LOCK */
        pthread_mutex_unlock(&t->pool->lock);

        /* and wake up dispatcher so it will handle it */
        ret = write(t->pool->sig_pipe[1], &dummy, 1);
        if (ret == -1) {
            GPERROR("Failed to signal dispatcher!");
        }
    }

    pthread_exit(NULL);
}