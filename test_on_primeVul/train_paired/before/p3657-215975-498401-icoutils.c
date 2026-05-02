extract_group_icon_cursor_resource(WinLibrary *fi, WinResource *wr, char *lang,
                                   int *ressize, bool is_icon)
{
	Win32CursorIconDir *icondir;
	Win32CursorIconFileDir *fileicondir;
	char *memory;
	int c, size, offset, skipped;

	/* get resource data and size */
	icondir = (Win32CursorIconDir *) get_resource_entry(fi, wr, &size);
	if (icondir == NULL) {
		/* get_resource_entry will print error */
		return NULL;
	}

	/* calculate total size of output file */
	RETURN_IF_BAD_POINTER(NULL, icondir->count);
	skipped = 0;
	for (c = 0 ; c < icondir->count ; c++) {
		int level;
	    	int iconsize;
		char name[14];
		WinResource *fwr;

		RETURN_IF_BAD_POINTER(NULL, icondir->entries[c]);
		/*printf("%d. bytes_in_res=%d width=%d height=%d planes=%d bit_count=%d\n", c,
			icondir->entries[c].bytes_in_res,
			(is_icon ? icondir->entries[c].res_info.icon.width : icondir->entries[c].res_info.cursor.width),
			(is_icon ? icondir->entries[c].res_info.icon.height : icondir->entries[c].res_info.cursor.height),
			icondir->entries[c].plane_count,
			icondir->entries[c].bit_count);*/

		/* find the corresponding icon resource */
		snprintf(name, sizeof(name)/sizeof(char), "-%d", icondir->entries[c].res_id);
		fwr = find_resource(fi, (is_icon ? "-3" : "-1"), name, lang, &level);
		if (fwr == NULL) {
			warn(_("%s: could not find `%s' in `%s' resource."),
			 	fi->name, &name[1], (is_icon ? "group_icon" : "group_cursor"));
			return NULL;
		}

		if (get_resource_entry(fi, fwr, &iconsize) != NULL) {
		    if (iconsize == 0) {
			warn(_("%s: icon resource `%s' is empty, skipping"), fi->name, name);
			skipped++;
			continue;
		    }
		    if (iconsize != icondir->entries[c].bytes_in_res) {
			warn(_("%s: mismatch of size in icon resource `%s' and group (%d vs %d)"), fi->name, name, iconsize, icondir->entries[c].bytes_in_res);
		    }
		    size += iconsize < icondir->entries[c].bytes_in_res ? icondir->entries[c].bytes_in_res : iconsize;

		    /* cursor resources have two additional WORDs that contain
		     * hotspot info */
		    if (!is_icon)
			size -= sizeof(uint16_t)*2;
		}
	}
	offset = sizeof(Win32CursorIconFileDir) + (icondir->count-skipped) * sizeof(Win32CursorIconFileDirEntry);
	size += offset;
	*ressize = size;

	/* allocate that much memory */
	memory = xmalloc(size);
	fileicondir = (Win32CursorIconFileDir *) memory;

	/* transfer Win32CursorIconDir structure members */
	fileicondir->reserved = icondir->reserved;
	fileicondir->type = icondir->type;
	fileicondir->count = icondir->count - skipped;

	/* transfer each cursor/icon: Win32CursorIconDirEntry and data */
	skipped = 0;
	for (c = 0 ; c < icondir->count ; c++) {
		int level;
		char name[14];
		WinResource *fwr;
		char *data;
	
		/* find the corresponding icon resource */
		snprintf(name, sizeof(name)/sizeof(char), "-%d", icondir->entries[c].res_id);
		fwr = find_resource(fi, (is_icon ? "-3" : "-1"), name, lang, &level);
		if (fwr == NULL) {
			warn(_("%s: could not find `%s' in `%s' resource."),
			 	fi->name, &name[1], (is_icon ? "group_icon" : "group_cursor"));
			return NULL;
		}

		/* get data and size of that resource */
		data = get_resource_entry(fi, fwr, &size);
		if (data == NULL) {
			/* get_resource_entry has printed error */
			return NULL;
		}
    	    	if (size == 0) {
		    skipped++;
		    continue;
		}

		/* copy ICONDIRENTRY (not including last dwImageOffset) */
		memcpy(&fileicondir->entries[c-skipped], &icondir->entries[c],
			sizeof(Win32CursorIconFileDirEntry)-sizeof(uint32_t));

		/* special treatment for cursors */
		if (!is_icon) {
			fileicondir->entries[c-skipped].width = icondir->entries[c].res_info.cursor.width;
			fileicondir->entries[c-skipped].height = icondir->entries[c].res_info.cursor.height / 2;
			fileicondir->entries[c-skipped].color_count = 0;
			fileicondir->entries[c-skipped].reserved = 0;
		}

		/* set image offset and increase it */
		fileicondir->entries[c-skipped].dib_offset = offset;

		/* transfer resource into file memory */
		if (size > icondir->entries[c].bytes_in_res)
			size = icondir->entries[c].bytes_in_res;
		if (is_icon) {
			memcpy(&memory[offset], data, size);
		} else {
			fileicondir->entries[c-skipped].hotspot_x = ((uint16_t *) data)[0];
			fileicondir->entries[c-skipped].hotspot_y = ((uint16_t *) data)[1];
			memcpy(&memory[offset], data+sizeof(uint16_t)*2,
				   size-sizeof(uint16_t)*2);
			offset -= sizeof(uint16_t)*2;
		}

		/* increase the offset pointer */
		offset += icondir->entries[c].bytes_in_res;
	}

	return (void *) memory;
}