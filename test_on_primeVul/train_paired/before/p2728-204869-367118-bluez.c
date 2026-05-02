static bool disconnect_cb(struct io *io, void *user_data)
{
	struct bt_att_chan *chan = user_data;
	struct bt_att *att = chan->att;
	int err;
	socklen_t len;

	len = sizeof(err);

	if (getsockopt(chan->fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
		util_debug(chan->att->debug_callback, chan->att->debug_data,
					"(chan %p) Failed to obtain disconnect"
					" error: %s", chan, strerror(errno));
		err = 0;
	}

	util_debug(chan->att->debug_callback, chan->att->debug_data,
					"Channel %p disconnected: %s",
					chan, strerror(err));

	/* Dettach channel */
	queue_remove(att->chans, chan);

	/* Notify request callbacks */
	queue_remove_all(att->req_queue, NULL, NULL, disc_att_send_op);
	queue_remove_all(att->ind_queue, NULL, NULL, disc_att_send_op);
	queue_remove_all(att->write_queue, NULL, NULL, disc_att_send_op);

	if (chan->pending_req) {
		disc_att_send_op(chan->pending_req);
		chan->pending_req = NULL;
	}

	if (chan->pending_ind) {
		disc_att_send_op(chan->pending_ind);
		chan->pending_ind = NULL;
	}

	bt_att_chan_free(chan);

	/* Don't run disconnect callback if there are channels left */
	if (!queue_isempty(att->chans))
		return false;

	bt_att_ref(att);

	queue_foreach(att->disconn_list, disconn_handler, INT_TO_PTR(err));

	bt_att_unregister_all(att);
	bt_att_unref(att);

	return false;
}