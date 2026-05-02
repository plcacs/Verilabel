static void sixpack_close(struct tty_struct *tty)
{
	struct sixpack *sp;

	write_lock_irq(&disc_data_lock);
	sp = tty->disc_data;
	tty->disc_data = NULL;
	write_unlock_irq(&disc_data_lock);
	if (!sp)
		return;

	/*
	 * We have now ensured that nobody can start using ap from now on, but
	 * we have to wait for all existing users to finish.
	 */
	if (!refcount_dec_and_test(&sp->refcnt))
		wait_for_completion(&sp->dead);

	/* We must stop the queue to avoid potentially scribbling
	 * on the free buffers. The sp->dead completion is not sufficient
	 * to protect us from sp->xbuff access.
	 */
	netif_stop_queue(sp->dev);

	del_timer_sync(&sp->tx_t);
	del_timer_sync(&sp->resync_t);

	unregister_netdev(sp->dev);

	/* Free all 6pack frame buffers after unreg. */
	kfree(sp->rbuff);
	kfree(sp->xbuff);

	free_netdev(sp->dev);
}