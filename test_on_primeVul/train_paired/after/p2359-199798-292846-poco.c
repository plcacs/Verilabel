bool ZipCommon::isValidPath(const std::string& path)
{
	try
	{
		if (Path(path, Path::PATH_UNIX).isAbsolute() || Path(path, Path::PATH_WINDOWS).isAbsolute())
			return false;
	}
	catch (...)
	{
		return false;
	}

	if (path == "..")
		return false;
	if ((path.size() >= 3) && path.compare(0, 3, "../") == 0)
		return false;
	if ((path.size() >= 3) && path.compare(0, 3, "..\\") == 0)
		return false;
	if (path.find("/../") != std::string::npos)
		return false;
	if (path.find("\\..\\") != std::string::npos)
		return false;
	if (path.find("/..\\") != std::string::npos)
		return false;
	if (path.find("\\../") != std::string::npos)
		return false;
	if ((path.size() >= 2) && path.compare(0, 2, "~/") == 0)
		return false;

	return true;
}