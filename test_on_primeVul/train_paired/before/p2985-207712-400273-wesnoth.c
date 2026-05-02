std::string get_wml_location(const std::string &filename, const std::string &current_dir)
{
	DBG_FS << "Looking for '" << filename << "'." << std::endl;

	assert(game_config::path.empty() == false);

	std::string result;

	if (filename.empty()) {
		LOG_FS << "  invalid filename" << std::endl;
		return result;
	}

	if (filename.find("..") != std::string::npos) {
		ERR_FS << "Illegal path '" << filename << "' (\"..\" not allowed)." << std::endl;
		return result;
	}

	bool already_found = false;

	if (filename[0] == '~')
	{
		// If the filename starts with '~', look in the user data directory.
		result = get_user_data_dir() + "/data/" + filename.substr(1);
		DBG_FS << "  trying '" << result << "'" << std::endl;

		already_found = file_exists(result) || is_directory(result);
	}
	else if (filename.size() >= 2 && filename[0] == '.' && filename[1] == '/')
	{
		// If the filename begins with a "./", look in the same directory
		// as the file currently being preprocessed.

		if (!current_dir.empty())
		{
			result = current_dir;
		}
		else
		{
			result = game_config::path;
		}

		result += filename.substr(2);
	}
	else if (!game_config::path.empty())
		result = game_config::path + "/data/" + filename;

	DBG_FS << "  trying '" << result << "'" << std::endl;

	if (result.empty() ||
	    (!already_found && !file_exists(result) && !is_directory(result)))
	{
		DBG_FS << "  not found" << std::endl;
		result.clear();
	}
	else
		DBG_FS << "  found: '" << result << "'" << std::endl;

	return result;
}