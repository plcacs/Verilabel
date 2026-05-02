static struct ext4_dir_entry_2 *do_split(handle_t *handle, struct inode *dir,
			struct buffer_head **bh,struct dx_frame *frame,
			struct dx_hash_info *hinfo)
{
	unsigned blocksize = dir->i_sb->s_blocksize;
	unsigned count, continued;
	struct buffer_head *bh2;
	ext4_lblk_t newblock;
	u32 hash2;
	struct dx_map_entry *map;
	char *data1 = (*bh)->b_data, *data2;
	unsigned split, move, size;
	struct ext4_dir_entry_2 *de = NULL, *de2;
	int	csum_size = 0;
	int	err = 0, i;

	if (ext4_has_metadata_csum(dir->i_sb))
		csum_size = sizeof(struct ext4_dir_entry_tail);

	bh2 = ext4_append(handle, dir, &newblock);
	if (IS_ERR(bh2)) {
		brelse(*bh);
		*bh = NULL;
		return (struct ext4_dir_entry_2 *) bh2;
	}

	BUFFER_TRACE(*bh, "get_write_access");
	err = ext4_journal_get_write_access(handle, *bh);
	if (err)
		goto journal_error;

	BUFFER_TRACE(frame->bh, "get_write_access");
	err = ext4_journal_get_write_access(handle, frame->bh);
	if (err)
		goto journal_error;

	data2 = bh2->b_data;

	/* create map in the end of data2 block */
	map = (struct dx_map_entry *) (data2 + blocksize);
	count = dx_make_map(dir, (struct ext4_dir_entry_2 *) data1,
			     blocksize, hinfo, map);
	map -= count;
	dx_sort_map(map, count);
	/* Split the existing block in the middle, size-wise */
	size = 0;
	move = 0;
	for (i = count-1; i >= 0; i--) {
		/* is more than half of this entry in 2nd half of the block? */
		if (size + map[i].size/2 > blocksize/2)
			break;
		size += map[i].size;
		move++;
	}
	/* map index at which we will split */
	split = count - move;
	hash2 = map[split].hash;
	continued = hash2 == map[split - 1].hash;
	dxtrace(printk(KERN_INFO "Split block %lu at %x, %i/%i\n",
			(unsigned long)dx_get_block(frame->at),
					hash2, split, count-split));

	/* Fancy dance to stay within two buffers */
	de2 = dx_move_dirents(data1, data2, map + split, count - split,
			      blocksize);
	de = dx_pack_dirents(data1, blocksize);
	de->rec_len = ext4_rec_len_to_disk(data1 + (blocksize - csum_size) -
					   (char *) de,
					   blocksize);
	de2->rec_len = ext4_rec_len_to_disk(data2 + (blocksize - csum_size) -
					    (char *) de2,
					    blocksize);
	if (csum_size) {
		ext4_initialize_dirent_tail(*bh, blocksize);
		ext4_initialize_dirent_tail(bh2, blocksize);
	}

	dxtrace(dx_show_leaf(dir, hinfo, (struct ext4_dir_entry_2 *) data1,
			blocksize, 1));
	dxtrace(dx_show_leaf(dir, hinfo, (struct ext4_dir_entry_2 *) data2,
			blocksize, 1));

	/* Which block gets the new entry? */
	if (hinfo->hash >= hash2) {
		swap(*bh, bh2);
		de = de2;
	}
	dx_insert_block(frame, hash2 + continued, newblock);
	err = ext4_handle_dirty_dirblock(handle, dir, bh2);
	if (err)
		goto journal_error;
	err = ext4_handle_dirty_dx_node(handle, dir, frame->bh);
	if (err)
		goto journal_error;
	brelse(bh2);
	dxtrace(dx_show_index("frame", frame->entries));
	return de;

journal_error:
	brelse(*bh);
	brelse(bh2);
	*bh = NULL;
	ext4_std_error(dir->i_sb, err);
	return ERR_PTR(err);
}