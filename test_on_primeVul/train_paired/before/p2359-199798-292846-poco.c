bool ZipCommon::isValidPath(const std::string& path)
{
	if (path == "..")
		return false;
	if (path.compare(0, 3, "../") == 0)
		return false;
	if (path.compare(0, 3, "..\\") == 0)
		return false;
	if (path.find("/..") != std::string::npos)
		return false;
	if (path.find("\\..") != std::string::npos)
		return false;
	return true;
}