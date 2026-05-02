static bool extractFileTo(zip* zip, const std::string &file, std::string& to,
                          char* buf, size_t len) {

  struct zip_stat zipStat;
  // Verify the file to be extracted is actually in the zip file
  if (zip_stat(zip, file.c_str(), 0, &zipStat) != 0) {
    return false;
  }

  auto clean_file = file;
  auto sep = std::string::npos;
  // Normally would just use std::string::rfind here, but if we want to be
  // consistent between Windows and Linux, even if techincally Linux won't use
  // backslash for a separator, we are checking for both types.
  int idx = file.length() - 1;
  while (idx >= 0) {
    if (FileUtil::isDirSeparator(file[idx])) {
      sep = idx;
      break;
    }
    idx--;
  }
  if (sep != std::string::npos) {
    // make_relative_path so we do not try to put files or dirs in bad
    // places. This securely "cleans" the file.
    clean_file = make_relative_path(file);
    std::string path = to + clean_file;
    bool is_dir_only = true;
    if (sep < file.length() - 1) { // not just a directory
      auto clean_file_dir = HHVM_FN(dirname)(clean_file);
      path = to + clean_file_dir.toCppString();
      is_dir_only = false;
    }

    // Make sure the directory path to extract to exists or can be created
    if (!HHVM_FN(is_dir)(path) && !HHVM_FN(mkdir)(path, 0777, true)) {
      return false;
    }

    // If we have a good directory to extract to above, we now check whether
    // the "file" parameter passed in is a directory or actually a file.
    if (is_dir_only) { // directory, like /usr/bin/
      return true;
    }
    // otherwise file is actually a file, so we actually extract.
  }

  // We have ensured that clean_file will be added to a relative path by the
  // time we get here.
  to.append(clean_file);

  auto zipFile = zip_fopen_index(zip, zipStat.index, 0);
  FAIL_IF_INVALID_PTR(zipFile);

  auto outFile = fopen(to.c_str(), "wb");
  if (outFile == nullptr) {
    zip_fclose(zipFile);
    return false;
  }

  for (auto n = zip_fread(zipFile, buf, len); n != 0;
       n = zip_fread(zipFile, buf, len)) {
    if (n < 0 || fwrite(buf, sizeof(char), n, outFile) != n) {
      zip_fclose(zipFile);
      fclose(outFile);
      remove(to.c_str());
      return false;
    }
  }

  zip_fclose(zipFile);
  if (fclose(outFile) != 0) {
    return false;
  }

  return true;
}