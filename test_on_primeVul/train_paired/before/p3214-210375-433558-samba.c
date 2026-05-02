static bool winbind_name_list_to_sid_string_list(struct pwb_context *ctx,
						 const char *user,
						 const char *name_list,
						 char *sid_list_buffer,
						 int sid_list_buffer_size)
{
	bool result = false;
	char *current_name = NULL;
	const char *search_location;
	const char *comma;
	int len;

	if (sid_list_buffer_size > 0) {
		sid_list_buffer[0] = 0;
	}

	search_location = name_list;
	while ((comma = strchr(search_location, ',')) != NULL) {
		current_name = strndup(search_location,
				       comma - search_location);
		if (NULL == current_name) {
			goto out;
		}

		if (!winbind_name_to_sid_string(ctx, user,
						current_name,
						sid_list_buffer,
						sid_list_buffer_size)) {
			/*
			 * If one group name failed, we must not fail
			 * the authentication totally, continue with
			 * the following group names. If user belongs to
			 * one of the valid groups, we must allow it
			 * login. -- BoYang
			 */

			_pam_log(ctx, LOG_INFO, "cannot convert group %s to sid, "
				 "check if group %s is valid group.", current_name,
				 current_name);
			_make_remark_format(ctx, PAM_TEXT_INFO, _("Cannot convert group %s "
					"to sid, please contact your administrator to see "
					"if group %s is valid."), current_name, current_name);
			SAFE_FREE(current_name);
			search_location = comma + 1;
			continue;
		}

		SAFE_FREE(current_name);

		if (!safe_append_string(sid_list_buffer, ",",
					sid_list_buffer_size)) {
			goto out;
		}

		search_location = comma + 1;
	}

	if (!winbind_name_to_sid_string(ctx, user, search_location,
					sid_list_buffer,
					sid_list_buffer_size)) {
		_pam_log(ctx, LOG_INFO, "cannot convert group %s to sid, "
			 "check if group %s is valid group.", search_location,
			 search_location);
		_make_remark_format(ctx, PAM_TEXT_INFO, _("Cannot convert group %s "
				"to sid, please contact your administrator to see "
				"if group %s is valid."), search_location, search_location);
		/*
		 * The lookup of the last name failed..
		 * It results in require_member_of_sid ends with ','
		 * It is malformated parameter here, overwrite the last ','.
		 */
		len = strlen(sid_list_buffer);
		if ((len != 0) && (sid_list_buffer[len - 1] == ',')) {
			sid_list_buffer[len - 1] = '\0';
		}
	}

	result = true;

out:
	SAFE_FREE(current_name);
	return result;
}