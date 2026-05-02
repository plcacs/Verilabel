static int qh_help(int sd, char *buf, unsigned int len)
{
	struct query_handler *qh = NULL;

	if (!*buf || !strcmp(buf, "help")) {
		nsock_printf_nul(sd,
			"  help <name>   show help for handler <name>\n"
			"  help list     list registered handlers\n");
		return 0;
	}

	if (!strcmp(buf, "list")) {

		for (qh = qhandlers; qh != NULL; qh = qh->next_qh) {
			nsock_printf(sd, "%-10s %s\n", qh->name, qh->description ? qh->description : "(No description available)");
		}

		nsock_printf(sd, "%c", 0);
		return 0;
	}

	qh = qh_find_handler(buf);
	if (qh == NULL) {

		nsock_printf_nul(sd, "No handler named '%s' is registered\n", buf);

	} else if (qh->handler(sd, "help", 4) > 200) {

		nsock_printf_nul(sd, "The handler %s doesn't have any help yet.", buf);
	}

	return 0;
}