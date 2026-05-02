int fcntl_dirnotify(int fd, struct file *filp, unsigned long arg)
{
	struct dnotify_struct *dn;
	struct dnotify_struct *odn;
	struct dnotify_struct **prev;
	struct inode *inode;
	fl_owner_t id = current->files;
	int error = 0;

	if ((arg & ~DN_MULTISHOT) == 0) {
		dnotify_flush(filp, id);
		return 0;
	}
	if (!dir_notify_enable)
		return -EINVAL;
	inode = filp->f_path.dentry->d_inode;
	if (!S_ISDIR(inode->i_mode))
		return -ENOTDIR;
	dn = kmem_cache_alloc(dn_cache, GFP_KERNEL);
	if (dn == NULL)
		return -ENOMEM;
	spin_lock(&inode->i_lock);
	prev = &inode->i_dnotify;
	while ((odn = *prev) != NULL) {
		if ((odn->dn_owner == id) && (odn->dn_filp == filp)) {
			odn->dn_fd = fd;
			odn->dn_mask |= arg;
			inode->i_dnotify_mask |= arg & ~DN_MULTISHOT;
			goto out_free;
		}
		prev = &odn->dn_next;
	}

	error = __f_setown(filp, task_pid(current), PIDTYPE_PID, 0);
	if (error)
		goto out_free;

	dn->dn_mask = arg;
	dn->dn_fd = fd;
	dn->dn_filp = filp;
	dn->dn_owner = id;
	inode->i_dnotify_mask |= arg & ~DN_MULTISHOT;
	dn->dn_next = inode->i_dnotify;
	inode->i_dnotify = dn;
	spin_unlock(&inode->i_lock);

	if (filp->f_op && filp->f_op->dir_notify)
		return filp->f_op->dir_notify(filp, arg);
	return 0;

out_free:
	spin_unlock(&inode->i_lock);
	kmem_cache_free(dn_cache, dn);
	return error;
}