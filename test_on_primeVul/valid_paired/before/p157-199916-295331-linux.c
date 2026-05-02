static int may_create_in_sticky(struct dentry * const dir,
				struct inode * const inode)
{
	if ((!sysctl_protected_fifos && S_ISFIFO(inode->i_mode)) ||
	    (!sysctl_protected_regular && S_ISREG(inode->i_mode)) ||
	    likely(!(dir->d_inode->i_mode & S_ISVTX)) ||
	    uid_eq(inode->i_uid, dir->d_inode->i_uid) ||
	    uid_eq(current_fsuid(), inode->i_uid))
		return 0;

	if (likely(dir->d_inode->i_mode & 0002) ||
	    (dir->d_inode->i_mode & 0020 &&
	     ((sysctl_protected_fifos >= 2 && S_ISFIFO(inode->i_mode)) ||
	      (sysctl_protected_regular >= 2 && S_ISREG(inode->i_mode))))) {
		const char *operation = S_ISFIFO(inode->i_mode) ?
					"sticky_create_fifo" :
					"sticky_create_regular";
		audit_log_path_denied(AUDIT_ANOM_CREAT, operation);
		return -EACCES;
	}
	return 0;
}