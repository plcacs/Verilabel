static struct dir *squashfs_opendir(unsigned int block_start, unsigned int offset,
	struct inode **i)
{
	squashfs_dir_header_2 dirh;
	char buffer[sizeof(squashfs_dir_entry_2) + SQUASHFS_NAME_LEN + 1]
		__attribute__((aligned));
	squashfs_dir_entry_2 *dire = (squashfs_dir_entry_2 *) buffer;
	long long start;
	int bytes = 0;
	int dir_count, size, res;
	struct dir_ent *ent, *cur_ent = NULL;
	struct dir *dir;

	TRACE("squashfs_opendir: inode start block %d, offset %d\n",
		block_start, offset);

	*i = read_inode(block_start, offset);

	dir = malloc(sizeof(struct dir));
	if(dir == NULL)
		MEM_ERROR();

	dir->dir_count = 0;
	dir->cur_entry = NULL;
	dir->mode = (*i)->mode;
	dir->uid = (*i)->uid;
	dir->guid = (*i)->gid;
	dir->mtime = (*i)->time;
	dir->xattr = (*i)->xattr;
	dir->dirs = NULL;

	if ((*i)->data == 0)
		/*
		 * if the directory is empty, skip the unnecessary
		 * lookup_entry, this fixes the corner case with
		 * completely empty filesystems where lookup_entry correctly
		 * returning -1 is incorrectly treated as an error
		 */
		return dir;

	start = sBlk.s.directory_table_start + (*i)->start;
	offset = (*i)->offset;
	size = (*i)->data + bytes;

	while(bytes < size) {
		if(swap) {
			squashfs_dir_header_2 sdirh;
			res = read_directory_data(&sdirh, &start, &offset, sizeof(sdirh));
			if(res)
				SQUASHFS_SWAP_DIR_HEADER_2(&dirh, &sdirh);
		} else
			res = read_directory_data(&dirh, &start, &offset, sizeof(dirh));

		if(res == FALSE)
			goto corrupted;

		dir_count = dirh.count + 1;
		TRACE("squashfs_opendir: Read directory header @ byte position "
			"%d, %d directory entries\n", bytes, dir_count);
		bytes += sizeof(dirh);

		/* dir_count should never be larger than SQUASHFS_DIR_COUNT */
		if(dir_count > SQUASHFS_DIR_COUNT) {
			ERROR("File system corrupted: too many entries in directory\n");
			goto corrupted;
		}

		while(dir_count--) {
			if(swap) {
				squashfs_dir_entry_2 sdire;
				res = read_directory_data(&sdire, &start,
					&offset, sizeof(sdire));
				if(res)
					SQUASHFS_SWAP_DIR_ENTRY_2(dire, &sdire);
			} else
				res = read_directory_data(dire, &start,
					&offset, sizeof(*dire));

			if(res == FALSE)
				goto corrupted;

			bytes += sizeof(*dire);

			/* size should never be SQUASHFS_NAME_LEN or larger */
			if(dire->size >= SQUASHFS_NAME_LEN) {
				ERROR("File system corrupted: filename too long\n");
				goto corrupted;
			}

			res = read_directory_data(dire->name, &start, &offset,
								dire->size + 1);

			if(res == FALSE)
				goto corrupted;

			dire->name[dire->size + 1] = '\0';

			/* check name for invalid characters (i.e /, ., ..) */
			if(check_name(dire->name, dire->size + 1) == FALSE) {
				ERROR("File system corrupted: invalid characters in name\n");
				goto corrupted;
			}

			TRACE("squashfs_opendir: directory entry %s, inode "
				"%d:%d, type %d\n", dire->name,
				dirh.start_block, dire->offset, dire->type);

			ent = malloc(sizeof(struct dir_ent));
			if(ent == NULL)
				MEM_ERROR();

			ent->name = strdup(dire->name);
			ent->start_block = dirh.start_block;
			ent->offset = dire->offset;
			ent->type = dire->type;
			ent->next = NULL;
			if(cur_ent == NULL)
				dir->dirs = ent;
			else
				cur_ent->next = ent;
			cur_ent = ent;
			dir->dir_count ++;
			bytes += dire->size + 1;
		}
	}

	return dir;

corrupted:
	squashfs_closedir(dir);
	return NULL;
}