static int open_cred_file(char *file_name,
			struct parsed_mount_info *parsed_info)
{
	char *line_buf = NULL;
	char *temp_val = NULL;
	FILE *fs = NULL;
	int i;
	const int line_buf_size = 4096;
	const int min_non_white = 10;

	i = toggle_dac_capability(0, 1);
	if (i)
		goto return_i;

	i = access(file_name, R_OK);
	if (i) {
		toggle_dac_capability(0, 0);
		i = errno;
		goto return_i;
	}

	fs = fopen(file_name, "r");
	if (fs == NULL) {
		toggle_dac_capability(0, 0);
		i = errno;
		goto return_i;
	}

	i = toggle_dac_capability(0, 0);
	if (i)
		goto return_i;

	line_buf = (char *)malloc(line_buf_size);
	if (line_buf == NULL) {
		i = EX_SYSERR;
		goto return_i;
	}

	/* parse line from credentials file */
	while (fgets(line_buf, line_buf_size, fs)) {
		/* eat leading white space */
		for (i = 0; i < line_buf_size - min_non_white + 1; i++) {
			if ((line_buf[i] != ' ') && (line_buf[i] != '\t'))
				break;
		}
		null_terminate_endl(line_buf);

		/* parse next token */
		switch (parse_cred_line(line_buf + i, &temp_val)) {
		case CRED_USER:
			strlcpy(parsed_info->username, temp_val,
				sizeof(parsed_info->username));
			parsed_info->got_user = 1;
			break;
		case CRED_PASS:
			i = set_password(parsed_info, temp_val);
			if (i)
				goto return_i;
			break;
		case CRED_DOM:
			if (parsed_info->verboseflag)
				fprintf(stderr, "domain=%s\n",
					temp_val);
			strlcpy(parsed_info->domain, temp_val,
				sizeof(parsed_info->domain));
			break;
		case CRED_UNPARSEABLE:
			if (parsed_info->verboseflag)
				fprintf(stderr, "Credential formatted "
					"incorrectly: %s\n",
					temp_val ? temp_val : "(null)");
			break;
		}
	}
	i = 0;
return_i:
	if (fs != NULL)
		fclose(fs);

	/* make sure passwords are scrubbed from memory */
	if (line_buf != NULL)
		memset(line_buf, 0, line_buf_size);
	free(line_buf);
	return i;
}