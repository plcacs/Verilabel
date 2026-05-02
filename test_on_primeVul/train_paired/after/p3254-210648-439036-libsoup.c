get_cookies (SoupCookieJar *jar, SoupURI *uri, gboolean for_http, gboolean copy_cookies)
{
	SoupCookieJarPrivate *priv;
	GSList *cookies, *domain_cookies;
	char *domain, *cur, *next_domain;
	GSList *new_head, *cookies_to_remove = NULL, *p;

	priv = soup_cookie_jar_get_instance_private (jar);

	if (!uri->host || !uri->host[0])
		return NULL;

	/* The logic here is a little weird, but the plan is that if
	 * uri->host is "www.foo.com", we will end up looking up
	 * cookies for ".www.foo.com", "www.foo.com", ".foo.com", and
	 * ".com", in that order. (Logic stolen from Mozilla.)
	 */
	cookies = NULL;
	domain = cur = g_strdup_printf (".%s", uri->host);
	next_domain = domain + 1;
	do {
		new_head = domain_cookies = g_hash_table_lookup (priv->domains, cur);
		while (domain_cookies) {
			GSList *next = domain_cookies->next;
			SoupCookie *cookie = domain_cookies->data;

			if (cookie->expires && soup_date_is_past (cookie->expires)) {
				cookies_to_remove = g_slist_append (cookies_to_remove,
								    cookie);
				new_head = g_slist_delete_link (new_head, domain_cookies);
				g_hash_table_insert (priv->domains,
						     g_strdup (cur),
						     new_head);
			} else if (soup_cookie_applies_to_uri (cookie, uri) &&
				   (for_http || !cookie->http_only))
				cookies = g_slist_append (cookies, copy_cookies ? soup_cookie_copy (cookie) : cookie);

			domain_cookies = next;
		}
		cur = next_domain;
		if (cur)
			next_domain = strchr (cur + 1, '.');
	} while (cur);
	g_free (domain);

	for (p = cookies_to_remove; p; p = p->next) {
		SoupCookie *cookie = p->data;

		soup_cookie_jar_changed (jar, cookie, NULL);
		soup_cookie_free (cookie);
	}
	g_slist_free (cookies_to_remove);

	return g_slist_sort_with_data (cookies, compare_cookies, jar);
}