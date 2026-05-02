std::string get_wml_location(const std::string &filename, const std::string &current_dir)
{
	DBG_FS << "Looking for '" << filename << "'.\n";

	std::string result;

	if (filename.empty()) {
		LOG_FS << "  invalid filename\n";
		return result;
	}

	if (filename.find("..") != std::string::npos) {
		ERR_FS << "Illegal path '" << filename << "' (\"..\" not allowed).\n";
		return result;
	}

	bool already_found = false;

	if (filename[0] == '~')
	{
		// If the filename starts with '~', look in the user data directory.
		result = get_user_data_dir() + "/data/" + filename.substr(1);
		DBG_FS << "  trying '" << result << "'\n";

		already_found = file_exists(result) || is_directory(result);
	}
	else if (filename.size() >= 2 && filename[0] == '.' && filename[1] == '/')
	{
		// If the filename begins with a "./", look in the same directory
		// as the file currrently being preprocessed.
		result = current_dir + filename.substr(2);
	}
	else if (!game_config::path.empty())
		result = game_config::path + "/data/" + filename;

	DBG_FS << "  trying '" << result << "'\n";

	if (result.empty() ||
	    (!already_found && !file_exists(result) && !is_directory(result)))
	{
		DBG_FS << "  not found\n";
		result.clear();
	}
	else
		DBG_FS << "  found: '" << result << "'\n";

	return result;
}