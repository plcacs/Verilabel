int nfs_atomic_open(struct inode *dir, struct dentry *dentry,
		    struct file *file, unsigned open_flags,
		    umode_t mode)
{
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(wq);
	struct nfs_open_context *ctx;
	struct dentry *res;
	struct iattr attr = { .ia_valid = ATTR_OPEN };
	struct inode *inode;
	unsigned int lookup_flags = 0;
	unsigned long dir_verifier;
	bool switched = false;
	int created = 0;
	int err;

	/* Expect a negative dentry */
	BUG_ON(d_inode(dentry));

	dfprintk(VFS, "NFS: atomic_open(%s/%lu), %pd\n",
			dir->i_sb->s_id, dir->i_ino, dentry);

	err = nfs_check_flags(open_flags);
	if (err)
		return err;

	/* NFS only supports OPEN on regular files */
	if ((open_flags & O_DIRECTORY)) {
		if (!d_in_lookup(dentry)) {
			/*
			 * Hashed negative dentry with O_DIRECTORY: dentry was
			 * revalidated and is fine, no need to perform lookup
			 * again
			 */
			return -ENOENT;
		}
		lookup_flags = LOOKUP_OPEN|LOOKUP_DIRECTORY;
		goto no_open;
	}

	if (dentry->d_name.len > NFS_SERVER(dir)->namelen)
		return -ENAMETOOLONG;

	if (open_flags & O_CREAT) {
		struct nfs_server *server = NFS_SERVER(dir);

		if (!(server->attr_bitmask[2] & FATTR4_WORD2_MODE_UMASK))
			mode &= ~current_umask();

		attr.ia_valid |= ATTR_MODE;
		attr.ia_mode = mode;
	}
	if (open_flags & O_TRUNC) {
		attr.ia_valid |= ATTR_SIZE;
		attr.ia_size = 0;
	}

	if (!(open_flags & O_CREAT) && !d_in_lookup(dentry)) {
		d_drop(dentry);
		switched = true;
		dentry = d_alloc_parallel(dentry->d_parent,
					  &dentry->d_name, &wq);
		if (IS_ERR(dentry))
			return PTR_ERR(dentry);
		if (unlikely(!d_in_lookup(dentry)))
			return finish_no_open(file, dentry);
	}

	ctx = create_nfs_open_context(dentry, open_flags, file);
	err = PTR_ERR(ctx);
	if (IS_ERR(ctx))
		goto out;

	trace_nfs_atomic_open_enter(dir, ctx, open_flags);
	inode = NFS_PROTO(dir)->open_context(dir, ctx, open_flags, &attr, &created);
	if (created)
		file->f_mode |= FMODE_CREATED;
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		trace_nfs_atomic_open_exit(dir, ctx, open_flags, err);
		put_nfs_open_context(ctx);
		d_drop(dentry);
		switch (err) {
		case -ENOENT:
			d_splice_alias(NULL, dentry);
			if (nfs_server_capable(dir, NFS_CAP_CASE_INSENSITIVE))
				dir_verifier = inode_peek_iversion_raw(dir);
			else
				dir_verifier = nfs_save_change_attribute(dir);
			nfs_set_verifier(dentry, dir_verifier);
			break;
		case -EISDIR:
		case -ENOTDIR:
			goto no_open;
		case -ELOOP:
			if (!(open_flags & O_NOFOLLOW))
				goto no_open;
			break;
			/* case -EINVAL: */
		default:
			break;
		}
		goto out;
	}

	err = nfs_finish_open(ctx, ctx->dentry, file, open_flags);
	trace_nfs_atomic_open_exit(dir, ctx, open_flags, err);
	put_nfs_open_context(ctx);
out:
	if (unlikely(switched)) {
		d_lookup_done(dentry);
		dput(dentry);
	}
	return err;

no_open:
	res = nfs_lookup(dir, dentry, lookup_flags);
	if (switched) {
		d_lookup_done(dentry);
		if (!res)
			res = dentry;
		else
			dput(dentry);
	}
	if (IS_ERR(res))
		return PTR_ERR(res);
	return finish_no_open(file, res);
}