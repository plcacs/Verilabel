bool ItemStackMetadata::setString(const std::string &name, const std::string &var)
{
	bool result = Metadata::setString(name, var);
	if (name == TOOLCAP_KEY)
		updateToolCapabilities();
	return result;
}