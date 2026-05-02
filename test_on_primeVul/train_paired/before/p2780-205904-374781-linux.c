xfs_attr_shortform_verify(
	struct xfs_inode		*ip)
{
	struct xfs_attr_shortform	*sfp;
	struct xfs_attr_sf_entry	*sfep;
	struct xfs_attr_sf_entry	*next_sfep;
	char				*endp;
	struct xfs_ifork		*ifp;
	int				i;
	int64_t				size;

	ASSERT(ip->i_afp->if_format == XFS_DINODE_FMT_LOCAL);
	ifp = XFS_IFORK_PTR(ip, XFS_ATTR_FORK);
	sfp = (struct xfs_attr_shortform *)ifp->if_u1.if_data;
	size = ifp->if_bytes;

	/*
	 * Give up if the attribute is way too short.
	 */
	if (size < sizeof(struct xfs_attr_sf_hdr))
		return __this_address;

	endp = (char *)sfp + size;

	/* Check all reported entries */
	sfep = &sfp->list[0];
	for (i = 0; i < sfp->hdr.count; i++) {
		/*
		 * struct xfs_attr_sf_entry has a variable length.
		 * Check the fixed-offset parts of the structure are
		 * within the data buffer.
		 */
		if (((char *)sfep + sizeof(*sfep)) >= endp)
			return __this_address;

		/* Don't allow names with known bad length. */
		if (sfep->namelen == 0)
			return __this_address;

		/*
		 * Check that the variable-length part of the structure is
		 * within the data buffer.  The next entry starts after the
		 * name component, so nextentry is an acceptable test.
		 */
		next_sfep = XFS_ATTR_SF_NEXTENTRY(sfep);
		if ((char *)next_sfep > endp)
			return __this_address;

		/*
		 * Check for unknown flags.  Short form doesn't support
		 * the incomplete or local bits, so we can use the namespace
		 * mask here.
		 */
		if (sfep->flags & ~XFS_ATTR_NSP_ONDISK_MASK)
			return __this_address;

		/*
		 * Check for invalid namespace combinations.  We only allow
		 * one namespace flag per xattr, so we can just count the
		 * bits (i.e. hweight) here.
		 */
		if (hweight8(sfep->flags & XFS_ATTR_NSP_ONDISK_MASK) > 1)
			return __this_address;

		sfep = next_sfep;
	}
	if ((void *)sfep != (void *)endp)
		return __this_address;

	return NULL;
}