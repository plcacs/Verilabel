__ext4_xattr_check_block(struct inode *inode, struct buffer_head *bh,
			 const char *function, unsigned int line)
{
	int error = -EFSCORRUPTED;

	if (buffer_verified(bh))
		return 0;

	if (BHDR(bh)->h_magic != cpu_to_le32(EXT4_XATTR_MAGIC) ||
	    BHDR(bh)->h_blocks != cpu_to_le32(1))
		goto errout;
	error = -EFSBADCRC;
	if (!ext4_xattr_block_csum_verify(inode, bh))
		goto errout;
	error = ext4_xattr_check_entries(BFIRST(bh), bh->b_data + bh->b_size,
					 bh->b_data);
errout:
	if (error)
		__ext4_error_inode(inode, function, line, 0,
				   "corrupted xattr block %llu",
				   (unsigned long long) bh->b_blocknr);
	else
		set_buffer_verified(bh);
	return error;
}