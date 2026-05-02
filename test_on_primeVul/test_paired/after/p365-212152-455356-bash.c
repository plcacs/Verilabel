disable_priv_mode ()
{
  int e;

#if HAVE_DECL_SETRESUID
  if (setresuid (current_user.uid, current_user.uid, current_user.uid) < 0)
#else
  if (setuid (current_user.uid) < 0)
#endif
    {
      e = errno;
      sys_error (_("cannot set uid to %d: effective uid %d"), current_user.uid, current_user.euid);
#if defined (EXIT_ON_SETUID_FAILURE)
      if (e == EAGAIN)
	exit (e);
#endif
    }
#if HAVE_DECL_SETRESGID
  if (setresgid (current_user.gid, current_user.gid, current_user.gid) < 0)
#else
  if (setgid (current_user.gid) < 0)
#endif
    sys_error (_("cannot set gid to %d: effective gid %d"), current_user.gid, current_user.egid);

  current_user.euid = current_user.uid;
  current_user.egid = current_user.gid;
}