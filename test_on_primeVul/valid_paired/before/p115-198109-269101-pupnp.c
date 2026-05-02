service_info *FindServiceControlURLPath(
	service_table *table, const char *controlURLPath)
{
	service_info *finger = NULL;
	uri_type parsed_url;
	uri_type parsed_url_in;

	if (table && parse_uri(controlURLPath,
			     strlen(controlURLPath),
			     &parsed_url_in) == HTTP_SUCCESS) {
		finger = table->serviceList;
		while (finger) {
			if (finger->controlURL) {
				if (parse_uri(finger->controlURL,
					    strlen(finger->controlURL),
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