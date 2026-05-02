set_fflags_platform(struct archive_write_disk *a, int fd, const char *name,
    mode_t mode, unsigned long set, unsigned long clear)
{
	int		 ret;
	int		 myfd = fd;
	int newflags, oldflags;
	/*
	 * Linux has no define for the flags that are only settable by
	 * the root user.  This code may seem a little complex, but
	 * there seem to be some Linux systems that lack these
	 * defines. (?)  The code below degrades reasonably gracefully
	 * if sf_mask is incomplete.
	 */
	const int sf_mask = 0
#if defined(FS_IMMUTABLE_FL)
	    | FS_IMMUTABLE_FL
#elif defined(EXT2_IMMUTABLE_FL)
	    | EXT2_IMMUTABLE_FL
#endif
#if defined(FS_APPEND_FL)
	    | FS_APPEND_FL
#elif defined(EXT2_APPEND_FL)
	    | EXT2_APPEND_FL
#endif
#if defined(FS_JOURNAL_DATA_FL)
	    | FS_JOURNAL_DATA_FL
#endif
	;

	if (set == 0 && clear == 0)
		return (ARCHIVE_OK);
	/* Only regular files and dirs can have flags. */
	if (!S_ISREG(mode) && !S_ISDIR(mode))
		return (ARCHIVE_OK);

	/* If we weren't given an fd, open it ourselves. */
	if (myfd < 0) {
		myfd = open(name, O_RDONLY | O_NONBLOCK | O_BINARY | O_CLOEXEC);
		__archive_ensure_cloexec_flag(myfd);
	}
	if (myfd < 0)
		return (ARCHIVE_OK);

	/*
	 * XXX As above, this would be way simpler if we didn't have
	 * to read the current flags from disk. XXX
	 */
	ret = ARCHIVE_OK;

	/* Read the current file flags. */
	if (ioctl(myfd,
#ifdef FS_IOC_GETFLAGS
	    FS_IOC_GETFLAGS,
#else
	    EXT2_IOC_GETFLAGS,
#endif
	    &oldflags) < 0)
		goto fail;

	/* Try setting the flags as given. */
	newflags = (oldflags & ~clear) | set;
	if (ioctl(myfd,
#ifdef FS_IOC_SETFLAGS
	    FS_IOC_SETFLAGS,
#else
	    EXT2_IOC_SETFLAGS,
#endif
	    &newflags) >= 0)
		goto cleanup;
	if (errno != EPERM)
		goto fail;

	/* If we couldn't set all the flags, try again with a subset. */
	newflags &= ~sf_mask;
	oldflags &= sf_mask;
	newflags |= oldflags;
	if (ioctl(myfd,
#ifdef FS_IOC_SETFLAGS
	    FS_IOC_SETFLAGS,
#else
	    EXT2_IOC_SETFLAGS,
#endif
	    &newflags) >= 0)
		goto cleanup;

	/* We couldn't set the flags, so report the failure. */
fail:
	archive_set_error(&a->archive, errno,
	    "Failed to set file flags");
	ret = ARCHIVE_WARN;
cleanup:
	if (fd < 0)
		close(myfd);
	return (ret);
}