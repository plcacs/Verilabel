SCM_DEFINE (scm_mkdir, "mkdir", 1, 1, 0,
            (SCM path, SCM mode),
	    "Create a new directory named by @var{path}.  If @var{mode} is omitted\n"
	    "then the permissions of the directory file are set using the current\n"
	    "umask.  Otherwise they are set to the decimal value specified with\n"
	    "@var{mode}.  The return value is unspecified.")
#define FUNC_NAME s_scm_mkdir
{
  int rv;
  mode_t mask;

  if (SCM_UNBNDP (mode))
    {
      mask = umask (0);
      umask (mask);
      STRING_SYSCALL (path, c_path, rv = mkdir (c_path, 0777 ^ mask));
    }
  else
    {
      STRING_SYSCALL (path, c_path, rv = mkdir (c_path, scm_to_uint (mode)));
    }
  if (rv != 0)
    SCM_SYSERROR;
  return SCM_UNSPECIFIED;
}