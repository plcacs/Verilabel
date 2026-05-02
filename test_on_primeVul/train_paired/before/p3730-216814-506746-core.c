static void part_write_body(const struct message_part *part,
			    string_t *str, bool extended)
{
	const struct message_part_data *data = part->data;
	bool text;

	i_assert(part->data != NULL);

	if ((part->flags & MESSAGE_PART_FLAG_MESSAGE_RFC822) != 0) {
		str_append(str, "\"message\" \"rfc822\"");
		text = FALSE;
	} else {
		/* "content type" "subtype" */
		if (data->content_type == NULL) {
			text = TRUE;
			str_append(str, "\"text\" \"plain\"");
		} else {
			text = (strcasecmp(data->content_type, "text") == 0);
			imap_append_string(str, data->content_type);
			str_append_c(str, ' ');
			imap_append_string(str, data->content_subtype);
		}
	}

	/* ("content type param key" "value" ...) */
	str_append_c(str, ' ');
	params_write(data->content_type_params,
		data->content_type_params_count, str, text);

	str_append_c(str, ' ');
	imap_append_nstring_nolf(str, data->content_id);
	str_append_c(str, ' ');
	imap_append_nstring_nolf(str, data->content_description);
	str_append_c(str, ' ');
	if (data->content_transfer_encoding != NULL)
		imap_append_string(str, data->content_transfer_encoding);
	else
		str_append(str, "\"7bit\"");
	str_printfa(str, " %"PRIuUOFF_T, part->body_size.virtual_size);

	if (text) {
		/* text/.. contains line count */
		str_printfa(str, " %u", part->body_size.lines);
	} else if ((part->flags & MESSAGE_PART_FLAG_MESSAGE_RFC822) != 0) {
		/* message/rfc822 contains envelope + body + line count */
		const struct message_part_data *child_data;

		i_assert(part->children != NULL);
		i_assert(part->children->next == NULL);

		child_data = part->children->data;

		str_append(str, " (");
		imap_envelope_write(child_data->envelope, str);
		str_append(str, ") ");

		part_write_bodystructure_siblings(part->children, str, extended);
		str_printfa(str, " %u", part->body_size.lines);
	}

	if (!extended)
		return;

	/* BODYSTRUCTURE data */

	/* "md5" ("content disposition" ("disposition" "params"))
	   ("body" "language" "params") "location" */
	str_append_c(str, ' ');
	imap_append_nstring_nolf(str, data->content_md5);
	part_write_bodystructure_common(data, str);
}