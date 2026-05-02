SCM_DEFINE (scm_mkdir, "mkdir", 1, 1, 0,
            (SCM path, SCM mode),
	    "Create a new directory named by @var{path}.  If @var{mode} is omitted\n"
	    "then the permissions of the directory are set to @code{#o777}\n"
	    "masked with the current umask (@pxref{Processes, @code{umask}}).\n"
	    "Otherwise they are set to the value specified with @var{mode}.\n"
	    "The return value is unspecified.")
#define FUNC_NAME s_scm_mkdir
{
  int rv;
  mode_t c_mode;

  c_mode = SCM_UNBNDP (mode) ? 0777 : scm_to_uint (mode);

  STRING_SYSCALL (path, c_path, rv = mkdir (c_path, c_mode));
  if (rv != 0)
    SCM_SYSERROR;

  return SCM_UNSPECIFIED;
}