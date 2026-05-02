struct vfsmount *clone_private_mount(const struct path *path)
{
	struct mount *old_mnt = real_mount(path->mnt);
	struct mount *new_mnt;

	if (IS_MNT_UNBINDABLE(old_mnt))
		return ERR_PTR(-EINVAL);

	new_mnt = clone_mnt(old_mnt, path->dentry, CL_PRIVATE);
	if (IS_ERR(new_mnt))
		return ERR_CAST(new_mnt);

	/* Longterm mount to be removed by kern_unmount*() */
	new_mnt->mnt_ns = MNT_NS_INTERNAL;

	return &new_mnt->mnt;
}