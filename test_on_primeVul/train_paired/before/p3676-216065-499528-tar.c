read_header (union block **return_block, struct tar_stat_info *info,
	     enum read_header_mode mode)
{
  union block *header;
  union block *header_copy;
  char *bp;
  union block *data_block;
  size_t size, written;
  union block *next_long_name = 0;
  union block *next_long_link = 0;
  size_t next_long_name_blocks = 0;
  size_t next_long_link_blocks = 0;

  while (1)
    {
      enum read_header status;

      header = find_next_block ();
      *return_block = header;
      if (!header)
	return HEADER_END_OF_FILE;

      if ((status = tar_checksum (header, false)) != HEADER_SUCCESS)
	return status;

      /* Good block.  Decode file size and return.  */

      if (header->header.typeflag == LNKTYPE)
	info->stat.st_size = 0;	/* links 0 size on tape */
      else
	{
	  info->stat.st_size = OFF_FROM_HEADER (header->header.size);
	  if (info->stat.st_size < 0)
	    return HEADER_FAILURE;
	}

      if (header->header.typeflag == GNUTYPE_LONGNAME
	  || header->header.typeflag == GNUTYPE_LONGLINK
	  || header->header.typeflag == XHDTYPE
	  || header->header.typeflag == XGLTYPE
	  || header->header.typeflag == SOLARIS_XHDTYPE)
	{
	  if (mode == read_header_x_raw)
	    return HEADER_SUCCESS_EXTENDED;
	  else if (header->header.typeflag == GNUTYPE_LONGNAME
		   || header->header.typeflag == GNUTYPE_LONGLINK)
	    {
	      size_t name_size = info->stat.st_size;
	      size_t n = name_size % BLOCKSIZE;
	      size = name_size + BLOCKSIZE;
	      if (n)
		size += BLOCKSIZE - n;

	      if (name_size != info->stat.st_size || size < name_size)
		xalloc_die ();

	      header_copy = xmalloc (size + 1);

	      if (header->header.typeflag == GNUTYPE_LONGNAME)
		{
		  free (next_long_name);
		  next_long_name = header_copy;
		  next_long_name_blocks = size / BLOCKSIZE;
		}
	      else
		{
		  free (next_long_link);
		  next_long_link = header_copy;
		  next_long_link_blocks = size / BLOCKSIZE;
		}

	      set_next_block_after (header);
	      *header_copy = *header;
	      bp = header_copy->buffer + BLOCKSIZE;

	      for (size -= BLOCKSIZE; size > 0; size -= written)
		{
		  data_block = find_next_block ();
		  if (! data_block)
		    {
		      ERROR ((0, 0, _("Unexpected EOF in archive")));
		      break;
		    }
		  written = available_space_after (data_block);
		  if (written > size)
		    written = size;

		  memcpy (bp, data_block->buffer, written);
		  bp += written;
		  set_next_block_after ((union block *)
					(data_block->buffer + written - 1));
		}

	      *bp = '\0';
	    }
	  else if (header->header.typeflag == XHDTYPE
		   || header->header.typeflag == SOLARIS_XHDTYPE)
	    xheader_read (&info->xhdr, header,
			  OFF_FROM_HEADER (header->header.size));
	  else if (header->header.typeflag == XGLTYPE)
	    {
	      struct xheader xhdr;

	      if (!recent_global_header)
		recent_global_header = xmalloc (sizeof *recent_global_header);
	      memcpy (recent_global_header, header,
		      sizeof *recent_global_header);
	      memset (&xhdr, 0, sizeof xhdr);
	      xheader_read (&xhdr, header,
			    OFF_FROM_HEADER (header->header.size));
	      xheader_decode_global (&xhdr);
	      xheader_destroy (&xhdr);
	      if (mode == read_header_x_global)
		return HEADER_SUCCESS_EXTENDED;
	    }

	  /* Loop!  */

	}
      else
	{
	  char const *name;
	  struct posix_header const *h = &header->header;
	  char namebuf[sizeof h->prefix + 1 + NAME_FIELD_SIZE + 1];

	  free (recent_long_name);

	  if (next_long_name)
	    {
	      name = next_long_name->buffer + BLOCKSIZE;
	      recent_long_name = next_long_name;
	      recent_long_name_blocks = next_long_name_blocks;
	    }
	  else
	    {
	      /* Accept file names as specified by POSIX.1-1996
                 section 10.1.1.  */
	      char *np = namebuf;

	      if (h->prefix[0] && strcmp (h->magic, TMAGIC) == 0)
		{
		  memcpy (np, h->prefix, sizeof h->prefix);
		  np[sizeof h->prefix] = '\0';
		  np += strlen (np);
		  *np++ = '/';
		}
	      memcpy (np, h->name, sizeof h->name);
	      np[sizeof h->name] = '\0';
	      name = namebuf;
	      recent_long_name = 0;
	      recent_long_name_blocks = 0;
	    }
	  assign_string (&info->orig_file_name, name);
	  assign_string (&info->file_name, name);
	  info->had_trailing_slash = strip_trailing_slashes (info->file_name);

	  free (recent_long_link);

	  if (next_long_link)
	    {
	      name = next_long_link->buffer + BLOCKSIZE;
	      recent_long_link = next_long_link;
	      recent_long_link_blocks = next_long_link_blocks;
	    }
	  else
	    {
	      memcpy (namebuf, h->linkname, sizeof h->linkname);
	      namebuf[sizeof h->linkname] = '\0';
	      name = namebuf;
	      recent_long_link = 0;
	      recent_long_link_blocks = 0;
	    }
	  assign_string (&info->link_name, name);

	  return HEADER_SUCCESS;
	}
    }
}