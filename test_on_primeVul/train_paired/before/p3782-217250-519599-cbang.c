std::string TarFileReader::extract(const string &_path) {
  if (_path.empty()) THROW("path cannot be empty");
  if (!hasMore()) THROW("No more tar files");

  string path = _path;
  if (SystemUtilities::isDirectory(path)) path += "/" + getFilename();

  LOG_DEBUG(5, "Extracting: " << path);

  return extract(*SystemUtilities::oopen(path));
}