static bool extractFileTo(zip* zip, const std::string &file, std::string& to,
                          char* buf, size_t len) {
  auto sep = file.rfind('/');
  if (sep != std::string::npos) {
    auto path = to + file.substr(0, sep);
    if (!HHVM_FN(is_dir)(path) && !HHVM_FN(mkdir)(path, 0777, true)) {
      return false;
    }

    if (sep == file.length() - 1) {
      return true;
    }
  }

  to.append(file);
  struct zip_stat zipStat;
  if (zip_stat(zip, file.c_str(), 0, &zipStat) != 0) {
    return false;
  }

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