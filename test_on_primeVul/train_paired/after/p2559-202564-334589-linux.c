static ssize_t ucma_migrate_id(struct ucma_file *new_file,
			       const char __user *inbuf,
			       int in_len, int out_len)
{
	struct rdma_ucm_migrate_id cmd;
	struct rdma_ucm_migrate_resp resp;
	struct ucma_event *uevent, *tmp;
	struct ucma_context *ctx;
	LIST_HEAD(event_list);
	struct fd f;
	struct ucma_file *cur_file;
	int ret = 0;

	if (copy_from_user(&cmd, inbuf, sizeof(cmd)))
		return -EFAULT;

	/* Get current fd to protect against it being closed */
	f = fdget(cmd.fd);
	if (!f.file)
		return -ENOENT;
	if (f.file->f_op != &ucma_fops) {
		ret = -EINVAL;
		goto file_put;
	}
	cur_file = f.file->private_data;

	/* Validate current fd and prevent destruction of id. */
	ctx = ucma_get_ctx(cur_file, cmd.id);
	if (IS_ERR(ctx)) {
		ret = PTR_ERR(ctx);
		goto file_put;
	}

	rdma_lock_handler(ctx->cm_id);
	/*
	 * ctx->file can only be changed under the handler & xa_lock. xa_load()
	 * must be checked again to ensure the ctx hasn't begun destruction
	 * since the ucma_get_ctx().
	 */
	xa_lock(&ctx_table);
	if (_ucma_find_context(cmd.id, cur_file) != ctx) {
		xa_unlock(&ctx_table);
		ret = -ENOENT;
		goto err_unlock;
	}
	ctx->file = new_file;
	xa_unlock(&ctx_table);

	mutex_lock(&cur_file->mut);
	list_del(&ctx->list);
	/*
	 * At this point lock_handler() prevents addition of new uevents for
	 * this ctx.
	 */
	list_for_each_entry_safe(uevent, tmp, &cur_file->event_list, list)
		if (uevent->ctx == ctx)
			list_move_tail(&uevent->list, &event_list);
	resp.events_reported = ctx->events_reported;
	mutex_unlock(&cur_file->mut);

	mutex_lock(&new_file->mut);
	list_add_tail(&ctx->list, &new_file->ctx_list);
	list_splice_tail(&event_list, &new_file->event_list);
	mutex_unlock(&new_file->mut);

	if (copy_to_user(u64_to_user_ptr(cmd.response),
			 &resp, sizeof(resp)))
		ret = -EFAULT;

err_unlock:
	rdma_unlock_handler(ctx->cm_id);
	ucma_put_ctx(ctx);
file_put:
	fdput(f);
	return ret;
}