static void do_change_user(FILE *fin, FILE *fout) {
  std::string uname;
  lwp_read(fin, uname);
  if (uname.length() > 0) {
    struct passwd *pw = getpwnam(uname.c_str());
    if (pw) {
      if (pw->pw_gid) {
        setgid(pw->pw_gid);
      }
      if (pw->pw_uid) {
        setuid(pw->pw_uid);
      }
    }
  }
}