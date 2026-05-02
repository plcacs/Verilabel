static inline void fuse_make_bad(struct inode *inode)
{
	remove_inode_hash(inode);
	set_bit(FUSE_I_BAD, &get_fuse_inode(inode)->state);
}