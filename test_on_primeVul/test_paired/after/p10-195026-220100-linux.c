nfs4_file_open(struct inode *inode, struct file *filp)
{
	struct nfs_open_context *ctx;
	struct dentry *dentry = file_dentry(filp);
	struct dentry *parent = NULL;
	struct inode *dir;
	unsigned openflags = filp->f_flags;
	struct iattr attr;
	int err;

	/*
	 * If no cached dentry exists or if it's negative, NFSv4 handled the
	 * opens in ->lookup() or ->create().
	 *
	 * We only get this far for a cached positive dentry.  We skipped
	 * revalidation, so handle it here by dropping the dentry and returning
	 * -EOPENSTALE.  The VFS will retry the lookup/create/open.
	 */

	dprintk("NFS: open file(%pd2)\n", dentry);

	err = nfs_check_flags(openflags);
	if (err)
		return err;

	if ((openflags & O_ACCMODE) == 3)
		openflags--;

	/* We can't create new files here */
	openflags &= ~(O_CREAT|O_EXCL);

	parent = dget_parent(dentry);
	dir = d_inode(parent);

	ctx = alloc_nfs_open_context(file_dentry(filp), filp->f_mode, filp);
	err = PTR_ERR(ctx);
	if (IS_ERR(ctx))
		goto out;

	attr.ia_valid = ATTR_OPEN;
	if (openflags & O_TRUNC) {
		attr.ia_valid |= ATTR_SIZE;
		attr.ia_size = 0;
		filemap_write_and_wait(inode->i_mapping);
	}

	inode = NFS_PROTO(dir)->open_context(dir, ctx, openflags, &attr, NULL);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		switch (err) {
		default:
			goto out_put_ctx;
		case -ENOENT:
		case -ESTALE:
		case -EISDIR:
		case -ENOTDIR:
		case -ELOOP:
			goto out_drop;
		}
	}
	if (inode != d_inode(dentry))
		goto out_drop;

	nfs_file_set_open_context(filp, ctx);
	nfs_fscache_open_file(inode, filp);
	err = 0;

out_put_ctx:
	put_nfs_open_context(ctx);
out:
	dput(parent);
	return err;

out_drop:
	d_drop(dentry);
	err = -EOPENSTALE;
	goto out_put_ctx;
}