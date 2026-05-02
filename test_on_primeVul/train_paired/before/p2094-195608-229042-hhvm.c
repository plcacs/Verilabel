bool Capability::ChangeUnixUser(uid_t uid) {
  if (setInitialCapabilities()) {
    struct passwd *pw;

    if ((pw = getpwuid(uid)) == nullptr) {
      Logger::Error("unable to getpwuid(%d): %s", uid,
                    folly::errnoStr(errno).c_str());
      return false;
    }

    if (pw->pw_gid == 0 || setgid(pw->pw_gid) < 0) {
      Logger::Error("unable to drop gid privs: %s",
                    folly::errnoStr(errno).c_str());
      return false;
    }

    if (uid == 0 || setuid(uid) < 0) {
      Logger::Error("unable to drop uid privs: %s",
                    folly::errnoStr(errno).c_str());
      return false;
    }

    if (!setMinimalCapabilities()) {
      Logger::Error("unable to set minimal server capabiltiies");
      return false;
    }
    return true;
  }
  return false;
}