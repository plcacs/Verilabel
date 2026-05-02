std::string get_wml_location(const std::string &filename, const std::string &current_dir)
{
	if (!is_legal_file(filename))
		return std::string();

	assert(game_config::path.empty() == false);

	path fpath(filename);
	path result;

	if (filename[0] == '~')
	{
		result /= get_user_data_path() / "data" / filename.substr(1);
		DBG_FS << "  trying '" << result.string() << "'\n";
	} else if (*fpath.begin() == ".") {
		if (!current_dir.empty()) {
			result /= path(current_dir);
		} else {
			result /= path(game_config::path) / "data";
		}

		result /= filename;
	} else if (!game_config::path.empty()) {
		result /= path(game_config::path) / "data" / filename;
	}
	if (result.empty() || !file_exists(result)) {
		DBG_FS << "  not found\n";
		result.clear();
	} else
		DBG_FS << "  found: '" << result.string() << "'\n";

	return result.string();
}