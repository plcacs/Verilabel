_gdata_service_build_session (void)
{
	SoupSession *session = soup_session_sync_new ();

#ifdef HAVE_GNOME
	soup_session_add_feature_by_type (session, SOUP_TYPE_GNOME_FEATURES_2_26);
#endif /* HAVE_GNOME */

	/* Log all libsoup traffic if debugging's turned on */
	if (_gdata_service_get_log_level () > GDATA_LOG_MESSAGES) {
		SoupLoggerLogLevel level;
		SoupLogger *logger;

		switch (_gdata_service_get_log_level ()) {
			case GDATA_LOG_FULL_UNREDACTED:
			case GDATA_LOG_FULL:
				level = SOUP_LOGGER_LOG_BODY;
				break;
			case GDATA_LOG_HEADERS:
				level = SOUP_LOGGER_LOG_HEADERS;
				break;
			case GDATA_LOG_MESSAGES:
			case GDATA_LOG_NONE:
			default:
				g_assert_not_reached ();
		}

		logger = soup_logger_new (level, -1);
		soup_logger_set_printer (logger, (SoupLoggerPrinter) soup_log_printer, NULL, NULL);

		soup_session_add_feature (session, SOUP_SESSION_FEATURE (logger));

		g_object_unref (logger);
	}

	return session;
}