void check_file_actlst(int *ifd, char *dfile, struct activity *act[], uint64_t flags,
		       struct file_magic *file_magic, struct file_header *file_hdr,
		       struct file_activity **file_actlst, unsigned int id_seq[],
		       int ignore, int *endian_mismatch, int *arch_64)
{
	int i, j, k, p;
	struct file_activity *fal;
	void *buffer = NULL;
	size_t bh_size = FILE_HEADER_SIZE;
	size_t ba_size = FILE_ACTIVITY_SIZE;

	/* Open sa data file and read its magic structure */
	if (sa_open_read_magic(ifd, dfile, file_magic, ignore, endian_mismatch, TRUE) < 0)
		/*
		 * Not current sysstat's format.
		 * Return now so that sadf -H can display at least
		 * file's version and magic number.
		 */
		return;

	/*
	 * We know now that we have a *compatible* sysstat datafile format
	 * (correct FORMAT_MAGIC value), and in this case, we should have
	 * checked header_size value. Anyway, with a corrupted datafile,
	 * this may not be the case. So check again.
	 */
	if ((file_magic->header_size <= MIN_FILE_HEADER_SIZE) ||
	    (file_magic->header_size > MAX_FILE_HEADER_SIZE)) {
#ifdef DEBUG
		fprintf(stderr, "%s: header_size=%u\n",
			__FUNCTION__, file_magic->header_size);
#endif
		goto format_error;
	}

	/* Allocate buffer for file_header structure */
	if (file_magic->header_size > FILE_HEADER_SIZE) {
		bh_size = file_magic->header_size;
	}
	SREALLOC(buffer, char, bh_size);

	/* Read sa data file standard header and allocate activity list */
	sa_fread(*ifd, buffer, (size_t) file_magic->header_size, HARD_SIZE, UEOF_STOP);
	/*
	 * Data file header size (file_magic->header_size) may be greater or
	 * smaller than FILE_HEADER_SIZE. Remap the fields of the file header
	 * then copy its contents to the expected structure.
	 */
	if (remap_struct(hdr_types_nr, file_magic->hdr_types_nr, buffer,
			 file_magic->header_size, FILE_HEADER_SIZE, bh_size) < 0)
		goto format_error;

	memcpy(file_hdr, buffer, FILE_HEADER_SIZE);
	free(buffer);
	buffer = NULL;

	/* Tell that data come from a 64 bit machine */
	*arch_64 = (file_hdr->sa_sizeof_long == SIZEOF_LONG_64BIT);

	/* Normalize endianness for file_hdr structure */
	if (*endian_mismatch) {
		swap_struct(hdr_types_nr, file_hdr, *arch_64);
	}

	/*
	 * Sanity checks.
	 * NB: Compare against MAX_NR_ACT and not NR_ACT because
	 * we are maybe reading a datafile from a future sysstat version
	 * with more activities than known today.
	 */
	if ((file_hdr->sa_act_nr > MAX_NR_ACT) ||
	    (file_hdr->act_size > MAX_FILE_ACTIVITY_SIZE) ||
	    (file_hdr->rec_size > MAX_RECORD_HEADER_SIZE) ||
	    (MAP_SIZE(file_hdr->act_types_nr) > file_hdr->act_size) ||
	    (MAP_SIZE(file_hdr->rec_types_nr) > file_hdr->rec_size)) {
#ifdef DEBUG
		fprintf(stderr, "%s: sa_act_nr=%d act_size=%u rec_size=%u map_size(act)=%u map_size(rec)=%u\n",
			__FUNCTION__, file_hdr->sa_act_nr, file_hdr->act_size, file_hdr->rec_size,
			MAP_SIZE(file_hdr->act_types_nr), MAP_SIZE(file_hdr->rec_types_nr));
#endif
		/* Maybe a "false positive" sysstat datafile? */
		goto format_error;
	}

	/* Allocate buffer for file_activity structures */
	if (file_hdr->act_size > FILE_ACTIVITY_SIZE) {
		ba_size = file_hdr->act_size;
	}
	SREALLOC(buffer, char, ba_size);
	SREALLOC(*file_actlst, struct file_activity, FILE_ACTIVITY_SIZE * file_hdr->sa_act_nr);
	fal = *file_actlst;

	/* Read activity list */
	j = 0;
	for (i = 0; i < file_hdr->sa_act_nr; i++, fal++) {

		/* Read current file_activity structure from file */
		sa_fread(*ifd, buffer, (size_t) file_hdr->act_size, HARD_SIZE, UEOF_STOP);
		/*
		* Data file_activity size (file_hdr->act_size) may be greater or
		* smaller than FILE_ACTIVITY_SIZE. Remap the fields of the file's structure
		* then copy its contents to the expected structure.
		*/
		if (remap_struct(act_types_nr, file_hdr->act_types_nr, buffer,
			     file_hdr->act_size, FILE_ACTIVITY_SIZE, ba_size) < 0)
			goto format_error;
		memcpy(fal, buffer, FILE_ACTIVITY_SIZE);

		/* Normalize endianness for file_activity structures */
		if (*endian_mismatch) {
			swap_struct(act_types_nr, fal, *arch_64);
		}

		/*
		 * Every activity, known or unknown, should have
		 * at least one item and sub-item, and a positive size value.
		 * Also check that the number of items and sub-items
		 * doesn't exceed a max value. This is necessary
		 * because we will use @nr and @nr2 to
		 * allocate memory to read the file contents. So we
		 * must make sure the file is not corrupted.
		 * NB: Another check will be made below for known
		 * activities which have each a specific max value.
		 */
		if ((fal->nr < 1) || (fal->nr2 < 1) ||
		    (fal->nr > NR_MAX) || (fal->nr2 > NR2_MAX) ||
		    (fal->size <= 0)) {
#ifdef DEBUG
			fprintf(stderr, "%s: id=%d nr=%d nr2=%d\n",
				__FUNCTION__, fal->id, fal->nr, fal->nr2);
#endif
			goto format_error;
		}

		if ((p = get_activity_position(act, fal->id, RESUME_IF_NOT_FOUND)) < 0)
			/* Unknown activity */
			continue;

		if (act[p]->magic != fal->magic) {
			/* Bad magical number */
			if (ignore) {
				/*
				 * This is how sadf -H knows that this
				 * activity has an unknown format.
				 */
				act[p]->magic = ACTIVITY_MAGIC_UNKNOWN;
			}
			else
				continue;
		}

		/* Check max value for known activities */
		if (fal->nr > act[p]->nr_max) {
#ifdef DEBUG
			fprintf(stderr, "%s: id=%d nr=%d nr_max=%d\n",
				__FUNCTION__, fal->id, fal->nr, act[p]->nr_max);
#endif
			goto format_error;
		}

		/*
		 * Number of fields of each type ("long long", or "long"
		 * or "int") composing the structure with statistics may
		 * only increase with new sysstat versions. Here, we may
		 * be reading a file created by current sysstat version,
		 * or by an older or a newer version.
		 */
		if (!(((fal->types_nr[0] >= act[p]->gtypes_nr[0]) &&
		     (fal->types_nr[1] >= act[p]->gtypes_nr[1]) &&
		     (fal->types_nr[2] >= act[p]->gtypes_nr[2]))
		     ||
		     ((fal->types_nr[0] <= act[p]->gtypes_nr[0]) &&
		     (fal->types_nr[1] <= act[p]->gtypes_nr[1]) &&
		     (fal->types_nr[2] <= act[p]->gtypes_nr[2]))) && !ignore) {
#ifdef DEBUG
			fprintf(stderr, "%s: id=%d file=%d,%d,%d activity=%d,%d,%d\n",
				__FUNCTION__, fal->id, fal->types_nr[0], fal->types_nr[1], fal->types_nr[2],
				act[p]->gtypes_nr[0], act[p]->gtypes_nr[1], act[p]->gtypes_nr[2]);
#endif
			goto format_error;
		}

		if (MAP_SIZE(fal->types_nr) > fal->size) {
#ifdef DEBUG
		fprintf(stderr, "%s: id=%d size=%u map_size=%u\n",
			__FUNCTION__, fal->id, fal->size, MAP_SIZE(fal->types_nr));
#endif
			goto format_error;
		}

		for (k = 0; k < 3; k++) {
			act[p]->ftypes_nr[k] = fal->types_nr[k];
		}

		if (fal->size > act[p]->msize) {
			act[p]->msize = fal->size;
		}

		act[p]->nr_ini = fal->nr;
		act[p]->nr2    = fal->nr2;
		act[p]->fsize  = fal->size;
		/*
		 * This is a known activity with a known format
		 * (magical number). Only such activities will be displayed.
		 * (Well, this may also be an unknown format if we have entered sadf -H.)
		 */
		id_seq[j++] = fal->id;
	}

	while (j < NR_ACT) {
		id_seq[j++] = 0;
	}

	free(buffer);

	/* Check that at least one activity selected by the user is available in file */
	for (i = 0; i < NR_ACT; i++) {

		if (!IS_SELECTED(act[i]->options))
			continue;

		/* Here is a selected activity: Does it exist in file? */
		fal = *file_actlst;
		for (j = 0; j < file_hdr->sa_act_nr; j++, fal++) {
			if (act[i]->id == fal->id)
				break;
		}
		if (j == file_hdr->sa_act_nr) {
			/* No: Unselect it */
			act[i]->options &= ~AO_SELECTED;
		}
	}

	/*
	 * None of selected activities exist in file: Abort.
	 * NB: Error is ignored if we only want to display
	 * datafile header (sadf -H).
	 */
	if (!get_activity_nr(act, AO_SELECTED, COUNT_ACTIVITIES) && !DISPLAY_HDR_ONLY(flags)) {
		fprintf(stderr, _("Requested activities not available in file %s\n"),
			dfile);
		close(*ifd);
		exit(1);
	}

	/*
	 * Check if there are some extra structures.
	 * We will just skip them as they are unknown for now.
	 */
	if (file_hdr->extra_next && (skip_extra_struct(*ifd, *endian_mismatch, *arch_64) < 0))
		goto format_error;

	return;

format_error:
	if (buffer) {
		free(buffer);
	}
	handle_invalid_sa_file(*ifd, file_magic, dfile, 0);
}