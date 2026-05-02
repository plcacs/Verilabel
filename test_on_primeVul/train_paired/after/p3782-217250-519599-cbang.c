std::string TarFileReader::extract(const string &_path) {
  if (_path.empty()) THROW("path cannot be empty");
  if (!hasMore()) THROW("No more tar files");

  string path = _path;
  if (SystemUtilities::isDirectory(path)) {
    path += "/" + getFilename();

    // Check that path is under the target directory
    string a = SystemUtilities::getCanonicalPath(_path);
    string b = SystemUtilities::getCanonicalPath(path);
    if (!String::startsWith(b, a))
      THROW("Tar path points outside of the extraction directory: " << path);
  }

  LOG_DEBUG(5, "Extracting: " << path);

  switch (getType()) {
  case NORMAL_FILE: case CONTIGUOUS_FILE:
    return extract(*SystemUtilities::oopen(path));
  case DIRECTORY: SystemUtilities::ensureDirectory(path); break;
  default: THROW("Unsupported tar file type " << getType());
  }

  return getFilename();
}