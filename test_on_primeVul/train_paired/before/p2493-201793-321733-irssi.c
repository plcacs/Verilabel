static void _cmd_window_show_opt(const char *data, int right)
{
	MAIN_WINDOW_REC *parent;
	WINDOW_REC *window;

	if (*data == '\0') cmd_return_error(CMDERR_NOT_ENOUGH_PARAMS);

	if (is_numeric(data, '\0')) {
		window = window_find_refnum(atoi(data));
		if (window == NULL) {
			printformat_window(active_win, MSGLEVEL_CLIENTERROR,
					   TXT_REFNUM_NOT_FOUND, data);
		}
	} else {
		window = window_find_item(active_win->active_server, data);
	}

	if (window == NULL || is_window_visible(window))
		return;

	if (WINDOW_GUI(window)->sticky) {
		if (!settings_get_bool("autounstick_windows")) {
			printformat_window(active_win, MSGLEVEL_CLIENTERROR,
					   TXT_CANT_SHOW_STICKY_WINDOWS);
			return;
		}
	}

	parent = mainwindow_create(right);
	parent->active = window;
	gui_window_reparent(window, parent);

	if (settings_get_bool("autostick_split_windows"))
		gui_window_set_sticky(window);

	active_mainwin = NULL;
	window_set_active(window);
}