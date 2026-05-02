std::string controller::bookmark(
		const std::string& url,
		const std::string& title,
		const std::string& description,
		const std::string& feed_title)
{
	std::string bookmark_cmd = cfg.get_configvalue("bookmark-cmd");
	bool is_interactive = cfg.get_configvalue_as_bool("bookmark-interactive");
	if (bookmark_cmd.length() > 0) {
		std::string cmdline = strprintf::fmt("%s '%s' %s %s %s",
		                                       bookmark_cmd,
		                                       utils::replace_all(url,"'", "%27"),
		                                       quote_empty(stfl::quote(title)),
		                                       quote_empty(stfl::quote(description)),
		                                       quote_empty(stfl::quote(feed_title)));

		LOG(level::DEBUG, "controller::bookmark: cmd = %s", cmdline);

		if (is_interactive) {
			v->push_empty_formaction();
			stfl::reset();
			utils::run_interactively(cmdline, "controller::bookmark");
			v->pop_current_formaction();
			return "";
		} else {
			char * my_argv[4];
			my_argv[0] = const_cast<char *>("/bin/sh");
			my_argv[1] = const_cast<char *>("-c");
			my_argv[2] = const_cast<char *>(cmdline.c_str());
			my_argv[3] = nullptr;
			return utils::run_program(my_argv, "");
		}
	} else {
		return _("bookmarking support is not configured. Please set the configuration variable `bookmark-cmd' accordingly.");
	}
}