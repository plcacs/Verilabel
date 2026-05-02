int udf_expand_file_adinicb(struct inode *inode)
{
	struct page *page;
	char *kaddr;
	struct udf_inode_info *iinfo = UDF_I(inode);
	int err;

	WARN_ON_ONCE(!inode_is_locked(inode));
	if (!iinfo->i_lenAlloc) {
		if (UDF_QUERY_FLAG(inode->i_sb, UDF_FLAG_USE_SHORT_AD))
			iinfo->i_alloc_type = ICBTAG_FLAG_AD_SHORT;
		else
			iinfo->i_alloc_type = ICBTAG_FLAG_AD_LONG;
		/* from now on we have normal address_space methods */
		inode->i_data.a_ops = &udf_aops;
		up_write(&iinfo->i_data_sem);
		mark_inode_dirty(inode);
		return 0;
	}
	/*
	 * Release i_data_sem so that we can lock a page - page lock ranks
	 * above i_data_sem. i_mutex still protects us against file changes.
	 */
	up_write(&iinfo->i_data_sem);

	page = find_or_create_page(inode->i_mapping, 0, GFP_NOFS);
	if (!page)
		return -ENOMEM;

	if (!PageUptodate(page)) {
		kaddr = kmap_atomic(page);
		memset(kaddr + iinfo->i_lenAlloc, 0x00,
		       PAGE_SIZE - iinfo->i_lenAlloc);
		memcpy(kaddr, iinfo->i_data + iinfo->i_lenEAttr,
			iinfo->i_lenAlloc);
		flush_dcache_page(page);
		SetPageUptodate(page);
		kunmap_atomic(kaddr);
	}
	down_write(&iinfo->i_data_sem);
	memset(iinfo->i_data + iinfo->i_lenEAttr, 0x00,
	       iinfo->i_lenAlloc);
	iinfo->i_lenAlloc = 0;
	if (UDF_QUERY_FLAG(inode->i_sb, UDF_FLAG_USE_SHORT_AD))
		iinfo->i_alloc_type = ICBTAG_FLAG_AD_SHORT;
	else
		iinfo->i_alloc_type = ICBTAG_FLAG_AD_LONG;
	/* from now on we have normal address_space methods */
	inode->i_data.a_ops = &udf_aops;
	set_page_dirty(page);
	unlock_page(page);
	up_write(&iinfo->i_data_sem);
	err = filemap_fdatawrite(inode->i_mapping);
	if (err) {
		/* Restore everything back so that we don't lose data... */
		lock_page(page);
		down_write(&iinfo->i_data_sem);
		kaddr = kmap_atomic(page);
		memcpy(iinfo->i_data + iinfo->i_lenEAttr, kaddr, inode->i_size);
		kunmap_atomic(kaddr);
		unlock_page(page);
		iinfo->i_alloc_type = ICBTAG_FLAG_AD_IN_ICB;
		inode->i_data.a_ops = &udf_adinicb_aops;
		iinfo->i_lenAlloc = inode->i_size;
		up_write(&iinfo->i_data_sem);
	}
	put_page(page);
	mark_inode_dirty(inode);

	return err;
}