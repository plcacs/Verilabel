static void mkiss_close(struct tty_struct *tty)
{
	struct mkiss *ax;

	write_lock_irq(&disc_data_lock);
	ax = tty->disc_data;
	tty->disc_data = NULL;
	write_unlock_irq(&disc_data_lock);

	if (!ax)
		return;

	/*
	 * We have now ensured that nobody can start using ap from now on, but
	 * we have to wait for all existing users to finish.
	 */
	if (!refcount_dec_and_test(&ax->refcnt))
		wait_for_completion(&ax->dead);
	/*
	 * Halt the transmit queue so that a new transmit cannot scribble
	 * on our buffers
	 */
	netif_stop_queue(ax->dev);

	ax->tty = NULL;

	unregister_netdev(ax->dev);

	/* Free all AX25 frame buffers after unreg. */
	kfree(ax->rbuff);
	kfree(ax->xbuff);

	free_netdev(ax->dev);
}