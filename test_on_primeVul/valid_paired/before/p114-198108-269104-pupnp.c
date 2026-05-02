service_info *FindServiceEventURLPath(
	service_table *table, const char *eventURLPath)
{
	service_info *finger = NULL;
	uri_type parsed_url;
	uri_type parsed_url_in;

	if (table &&
		parse_uri(eventURLPath, strlen(eventURLPath), &parsed_url_in) ==
			HTTP_SUCCESS) {
		finger = table->serviceList;
		while (finger) {
			if (finger->eventURL) {
				if (parse_uri(finger->eventURL,
					    strlen(finger->eventURL),
					    &parsed_url) == HTTP_SUCCESS) {
					if (!token_cmp(&parsed_url.pathquery,
						    &parsed_url_in.pathquery)) {
						return finger;
					}
				}
			}
			finger = finger->next;
		}
	}

	return NULL;
}