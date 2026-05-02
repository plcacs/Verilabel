static NTSTATUS vfswrap_fsctl(struct vfs_handle_struct *handle,
			      struct files_struct *fsp,
			      TALLOC_CTX *ctx,
			      uint32_t function,
			      uint16_t req_flags, /* Needed for UNICODE ... */
			      const uint8_t *_in_data,
			      uint32_t in_len,
			      uint8_t **_out_data,
			      uint32_t max_out_len,
			      uint32_t *out_len)
{
	const char *in_data = (const char *)_in_data;
	char **out_data = (char **)_out_data;

	switch (function) {
	case FSCTL_SET_SPARSE:
	{
		bool set_sparse = true;
		NTSTATUS status;

		if (in_len >= 1 && in_data[0] == 0) {
			set_sparse = false;
		}

		status = file_set_sparse(handle->conn, fsp, set_sparse);
		
		DEBUG(NT_STATUS_IS_OK(status) ? 10 : 9,
		      ("FSCTL_SET_SPARSE: fname[%s] set[%u] - %s\n",
		       smb_fname_str_dbg(fsp->fsp_name), set_sparse, 
		       nt_errstr(status)));

		return status;
	}

	case FSCTL_CREATE_OR_GET_OBJECT_ID:
	{
		unsigned char objid[16];
		char *return_data = NULL;

		/* This should return the object-id on this file.
		 * I think I'll make this be the inode+dev. JRA.
		 */

		DEBUG(10,("FSCTL_CREATE_OR_GET_OBJECT_ID: called on %s\n",
			  fsp_fnum_dbg(fsp)));

		*out_len = (max_out_len >= 64) ? 64 : max_out_len;
		/* Hmmm, will this cause problems if less data asked for? */
		return_data = talloc_array(ctx, char, 64);
		if (return_data == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		/* For backwards compatibility only store the dev/inode. */
		push_file_id_16(return_data, &fsp->file_id);
		memcpy(return_data+16,create_volume_objectid(fsp->conn,objid),16);
		push_file_id_16(return_data+32, &fsp->file_id);
		*out_data = return_data;
		return NT_STATUS_OK;
	}

	case FSCTL_GET_REPARSE_POINT:
	{
		/* Fail it with STATUS_NOT_A_REPARSE_POINT */
		DEBUG(10, ("FSCTL_GET_REPARSE_POINT: called on %s. "
			   "Status: NOT_IMPLEMENTED\n", fsp_fnum_dbg(fsp)));
		return NT_STATUS_NOT_A_REPARSE_POINT;
	}

	case FSCTL_SET_REPARSE_POINT:
	{
		/* Fail it with STATUS_NOT_A_REPARSE_POINT */
		DEBUG(10, ("FSCTL_SET_REPARSE_POINT: called on %s. "
			   "Status: NOT_IMPLEMENTED\n", fsp_fnum_dbg(fsp)));
		return NT_STATUS_NOT_A_REPARSE_POINT;
	}

	case FSCTL_GET_SHADOW_COPY_DATA:
	{
		/*
		 * This is called to retrieve the number of Shadow Copies (a.k.a. snapshots)
		 * and return their volume names.  If max_data_count is 16, then it is just
		 * asking for the number of volumes and length of the combined names.
		 *
		 * pdata is the data allocated by our caller, but that uses
		 * total_data_count (which is 0 in our case) rather than max_data_count.
		 * Allocate the correct amount and return the pointer to let
		 * it be deallocated when we return.
		 */
		struct shadow_copy_data *shadow_data = NULL;
		bool labels = False;
		uint32 labels_data_count = 0;
		uint32 i;
		char *cur_pdata = NULL;

		if (max_out_len < 16) {
			DEBUG(0,("FSCTL_GET_SHADOW_COPY_DATA: max_data_count(%u) < 16 is invalid!\n",
				max_out_len));
			return NT_STATUS_INVALID_PARAMETER;
		}

		if (max_out_len > 16) {
			labels = True;
		}

		shadow_data = talloc_zero(ctx, struct shadow_copy_data);
		if (shadow_data == NULL) {
			DEBUG(0,("TALLOC_ZERO() failed!\n"));
			return NT_STATUS_NO_MEMORY;
		}

		/*
		 * Call the VFS routine to actually do the work.
		 */
		if (SMB_VFS_GET_SHADOW_COPY_DATA(fsp, shadow_data, labels)!=0) {
			TALLOC_FREE(shadow_data);
			if (errno == ENOSYS) {
				DEBUG(5,("FSCTL_GET_SHADOW_COPY_DATA: connectpath %s, not supported.\n", 
					fsp->conn->connectpath));
				return NT_STATUS_NOT_SUPPORTED;
			} else {
				DEBUG(0,("FSCTL_GET_SHADOW_COPY_DATA: connectpath %s, failed.\n", 
					fsp->conn->connectpath));
				return NT_STATUS_UNSUCCESSFUL;
			}
		}

		labels_data_count = (shadow_data->num_volumes * 2 * 
					sizeof(SHADOW_COPY_LABEL)) + 2;

		if (!labels) {
			*out_len = 16;
		} else {
			*out_len = 12 + labels_data_count + 4;
		}

		if (max_out_len < *out_len) {
			DEBUG(0,("FSCTL_GET_SHADOW_COPY_DATA: max_data_count(%u) too small (%u) bytes needed!\n",
				max_out_len, *out_len));
			TALLOC_FREE(shadow_data);
			return NT_STATUS_BUFFER_TOO_SMALL;
		}

		cur_pdata = talloc_zero_array(ctx, char, *out_len);
		if (cur_pdata == NULL) {
			TALLOC_FREE(shadow_data);
			return NT_STATUS_NO_MEMORY;
		}

		*out_data = cur_pdata;

		/* num_volumes 4 bytes */
		SIVAL(cur_pdata, 0, shadow_data->num_volumes);

		if (labels) {
			/* num_labels 4 bytes */
			SIVAL(cur_pdata, 4, shadow_data->num_volumes);
		}

		/* needed_data_count 4 bytes */
		SIVAL(cur_pdata, 8, labels_data_count + 4);

		cur_pdata += 12;

		DEBUG(10,("FSCTL_GET_SHADOW_COPY_DATA: %u volumes for path[%s].\n",
			  shadow_data->num_volumes, fsp_str_dbg(fsp)));
		if (labels && shadow_data->labels) {
			for (i=0; i<shadow_data->num_volumes; i++) {
				srvstr_push(cur_pdata, req_flags,
					    cur_pdata, shadow_data->labels[i],
					    2 * sizeof(SHADOW_COPY_LABEL),
					    STR_UNICODE|STR_TERMINATE);
				cur_pdata += 2 * sizeof(SHADOW_COPY_LABEL);
				DEBUGADD(10,("Label[%u]: '%s'\n",i,shadow_data->labels[i]));
			}
		}

		TALLOC_FREE(shadow_data);

		return NT_STATUS_OK;
	}

	case FSCTL_FIND_FILES_BY_SID:
	{
		/* pretend this succeeded -
		 *
		 * we have to send back a list with all files owned by this SID
		 *
		 * but I have to check that --metze
		 */
		struct dom_sid sid;
		uid_t uid;
		size_t sid_len;

		DEBUG(10, ("FSCTL_FIND_FILES_BY_SID: called on %s\n",
			   fsp_fnum_dbg(fsp)));

		if (in_len < 8) {
			/* NT_STATUS_BUFFER_TOO_SMALL maybe? */
			return NT_STATUS_INVALID_PARAMETER;
		}

		sid_len = MIN(in_len - 4,SID_MAX_SIZE);

		/* unknown 4 bytes: this is not the length of the sid :-(  */
		/*unknown = IVAL(pdata,0);*/

		if (!sid_parse(in_data + 4, sid_len, &sid)) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		DEBUGADD(10, ("for SID: %s\n", sid_string_dbg(&sid)));

		if (!sid_to_uid(&sid, &uid)) {
			DEBUG(0,("sid_to_uid: failed, sid[%s] sid_len[%lu]\n",
				 sid_string_dbg(&sid),
				 (unsigned long)sid_len));
			uid = (-1);
		}

		/* we can take a look at the find source :-)
		 *
		 * find ./ -uid $uid  -name '*'   is what we need here
		 *
		 *
		 * and send 4bytes len and then NULL terminated unicode strings
		 * for each file
		 *
		 * but I don't know how to deal with the paged results
		 * (maybe we can hang the result anywhere in the fsp struct)
		 *
		 * but I don't know how to deal with the paged results
		 * (maybe we can hang the result anywhere in the fsp struct)
		 *
		 * we don't send all files at once
		 * and at the next we should *not* start from the beginning,
		 * so we have to cache the result
		 *
		 * --metze
		 */

		/* this works for now... */
		return NT_STATUS_OK;
	}

	case FSCTL_QUERY_ALLOCATED_RANGES:
	{
		/* FIXME: This is just a dummy reply, telling that all of the
		 * file is allocated. MKS cp needs that.
		 * Adding the real allocated ranges via FIEMAP on Linux
		 * and SEEK_DATA/SEEK_HOLE on Solaris is needed to make
		 * this FSCTL correct for sparse files.
		 */
		NTSTATUS status;
		uint64_t offset, length;
		char *out_data_tmp = NULL;

		if (in_len != 16) {
			DEBUG(0,("FSCTL_QUERY_ALLOCATED_RANGES: data_count(%u) != 16 is invalid!\n",
				in_len));
			return NT_STATUS_INVALID_PARAMETER;
		}

		if (max_out_len < 16) {
			DEBUG(0,("FSCTL_QUERY_ALLOCATED_RANGES: max_out_len (%u) < 16 is invalid!\n",
				max_out_len));
			return NT_STATUS_INVALID_PARAMETER;
		}

		offset = BVAL(in_data,0);
		length = BVAL(in_data,8);

		if (offset + length < offset) {
			/* No 64-bit integer wrap. */
			return NT_STATUS_INVALID_PARAMETER;
		}

		/* Shouldn't this be SMB_VFS_STAT ... ? */
		status = vfs_stat_fsp(fsp);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		*out_len = 16;
		out_data_tmp = talloc_array(ctx, char, *out_len);
		if (out_data_tmp == NULL) {
			DEBUG(10, ("unable to allocate memory for response\n"));
			return NT_STATUS_NO_MEMORY;
		}

		if (offset > fsp->fsp_name->st.st_ex_size ||
				fsp->fsp_name->st.st_ex_size == 0 ||
				length == 0) {
			memset(out_data_tmp, 0, *out_len);
		} else {
			uint64_t end = offset + length;
			end = MIN(end, fsp->fsp_name->st.st_ex_size);
			SBVAL(out_data_tmp, 0, 0);
			SBVAL(out_data_tmp, 8, end);
		}

		*out_data = out_data_tmp;

		return NT_STATUS_OK;
	}

	case FSCTL_IS_VOLUME_DIRTY:
	{
		DEBUG(10,("FSCTL_IS_VOLUME_DIRTY: called on %s "
			  "(but remotely not supported)\n", fsp_fnum_dbg(fsp)));
		/*
		 * http://msdn.microsoft.com/en-us/library/cc232128%28PROT.10%29.aspx
		 * says we have to respond with NT_STATUS_INVALID_PARAMETER
		 */
		return NT_STATUS_INVALID_PARAMETER;
	}

	default:
		/* 
		 * Only print once ... unfortunately there could be lots of
		 * different FSCTLs that are called.
		 */
		if (!vfswrap_logged_ioctl_message) {
			vfswrap_logged_ioctl_message = true;
			DEBUG(2, ("%s (0x%x): Currently not implemented.\n",
			__func__, function));
		}
	}

	return NT_STATUS_NOT_SUPPORTED;
}