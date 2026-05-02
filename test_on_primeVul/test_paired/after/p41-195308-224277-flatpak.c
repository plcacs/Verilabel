setup_seccomp (FlatpakBwrap   *bwrap,
               const char     *arch,
               gulong          allowed_personality,
               FlatpakRunFlags run_flags,
               GError        **error)
{
  gboolean multiarch = (run_flags & FLATPAK_RUN_FLAG_MULTIARCH) != 0;
  gboolean devel = (run_flags & FLATPAK_RUN_FLAG_DEVEL) != 0;

  __attribute__((cleanup (cleanup_seccomp))) scmp_filter_ctx seccomp = NULL;

  /**** BEGIN NOTE ON CODE SHARING
   *
   * There are today a number of different Linux container
   * implementations.  That will likely continue for long into the
   * future.  But we can still try to share code, and it's important
   * to do so because it affects what library and application writers
   * can do, and we should support code portability between different
   * container tools.
   *
   * This syscall blocklist is copied from linux-user-chroot, which was in turn
   * clearly influenced by the Sandstorm.io blocklist.
   *
   * If you make any changes here, I suggest sending the changes along
   * to other sandbox maintainers.  Using the libseccomp list is also
   * an appropriate venue:
   * https://groups.google.com/forum/#!forum/libseccomp
   *
   * A non-exhaustive list of links to container tooling that might
   * want to share this blocklist:
   *
   *  https://github.com/sandstorm-io/sandstorm
   *    in src/sandstorm/supervisor.c++
   *  https://github.com/flatpak/flatpak.git
   *    in common/flatpak-run.c
   *  https://git.gnome.org/browse/linux-user-chroot
   *    in src/setup-seccomp.c
   *
   * Other useful resources:
   * https://github.com/systemd/systemd/blob/HEAD/src/shared/seccomp-util.c
   * https://github.com/moby/moby/blob/HEAD/profiles/seccomp/default.json
   *
   **** END NOTE ON CODE SHARING
   */
  struct
  {
    int                  scall;
    int                  errnum;
    struct scmp_arg_cmp *arg;
  } syscall_blocklist[] = {
    /* Block dmesg */
    {SCMP_SYS (syslog), EPERM},
    /* Useless old syscall */
    {SCMP_SYS (uselib), EPERM},
    /* Don't allow disabling accounting */
    {SCMP_SYS (acct), EPERM},
    /* 16-bit code is unnecessary in the sandbox, and modify_ldt is a
       historic source of interesting information leaks. */
    {SCMP_SYS (modify_ldt), EPERM},
    /* Don't allow reading current quota use */
    {SCMP_SYS (quotactl), EPERM},

    /* Don't allow access to the kernel keyring */
    {SCMP_SYS (add_key), EPERM},
    {SCMP_SYS (keyctl), EPERM},
    {SCMP_SYS (request_key), EPERM},

    /* Scary VM/NUMA ops */
    {SCMP_SYS (move_pages), EPERM},
    {SCMP_SYS (mbind), EPERM},
    {SCMP_SYS (get_mempolicy), EPERM},
    {SCMP_SYS (set_mempolicy), EPERM},
    {SCMP_SYS (migrate_pages), EPERM},

    /* Don't allow subnamespace setups: */
    {SCMP_SYS (unshare), EPERM},
    {SCMP_SYS (setns), EPERM},
    {SCMP_SYS (mount), EPERM},
    {SCMP_SYS (umount), EPERM},
    {SCMP_SYS (umount2), EPERM},
    {SCMP_SYS (pivot_root), EPERM},
    {SCMP_SYS (chroot), EPERM},
#if defined(__s390__) || defined(__s390x__) || defined(__CRIS__)
    /* Architectures with CONFIG_CLONE_BACKWARDS2: the child stack
     * and flags arguments are reversed so the flags come second */
    {SCMP_SYS (clone), EPERM, &SCMP_A1 (SCMP_CMP_MASKED_EQ, CLONE_NEWUSER, CLONE_NEWUSER)},
#else
    /* Normally the flags come first */
    {SCMP_SYS (clone), EPERM, &SCMP_A0 (SCMP_CMP_MASKED_EQ, CLONE_NEWUSER, CLONE_NEWUSER)},
#endif

    /* Don't allow faking input to the controlling tty (CVE-2017-5226) */
    {SCMP_SYS (ioctl), EPERM, &SCMP_A1 (SCMP_CMP_MASKED_EQ, 0xFFFFFFFFu, (int) TIOCSTI)},

    /* seccomp can't look into clone3()'s struct clone_args to check whether
     * the flags are OK, so we have no choice but to block clone3().
     * Return ENOSYS so user-space will fall back to clone().
     * (GHSA-67h7-w3jq-vh4q; see also https://github.com/moby/moby/commit/9f6b562d) */
    {SCMP_SYS (clone3), ENOSYS},

    /* New mount manipulation APIs can also change our VFS. There's no
     * legitimate reason to do these in the sandbox, so block all of them
     * rather than thinking about which ones might be dangerous.
     * (GHSA-67h7-w3jq-vh4q) */
    {SCMP_SYS (open_tree), ENOSYS},
    {SCMP_SYS (move_mount), ENOSYS},
    {SCMP_SYS (fsopen), ENOSYS},
    {SCMP_SYS (fsconfig), ENOSYS},
    {SCMP_SYS (fsmount), ENOSYS},
    {SCMP_SYS (fspick), ENOSYS},
    {SCMP_SYS (mount_setattr), ENOSYS},
  };

  struct
  {
    int                  scall;
    int                  errnum;
    struct scmp_arg_cmp *arg;
  } syscall_nondevel_blocklist[] = {
    /* Profiling operations; we expect these to be done by tools from outside
     * the sandbox.  In particular perf has been the source of many CVEs.
     */
    {SCMP_SYS (perf_event_open), EPERM},
    /* Don't allow you to switch to bsd emulation or whatnot */
    {SCMP_SYS (personality), EPERM, &SCMP_A0 (SCMP_CMP_NE, allowed_personality)},
    {SCMP_SYS (ptrace), EPERM}
  };
  /* Blocklist all but unix, inet, inet6 and netlink */
  struct
  {
    int             family;
    FlatpakRunFlags flags_mask;
  } socket_family_allowlist[] = {
    /* NOTE: Keep in numerical order */
    { AF_UNSPEC, 0 },
    { AF_LOCAL, 0 },
    { AF_INET, 0 },
    { AF_INET6, 0 },
    { AF_NETLINK, 0 },
    { AF_CAN, FLATPAK_RUN_FLAG_CANBUS },
    { AF_BLUETOOTH, FLATPAK_RUN_FLAG_BLUETOOTH },
  };
  int last_allowed_family;
  int i, r;
  g_auto(GLnxTmpfile) seccomp_tmpf  = { 0, };

  seccomp = seccomp_init (SCMP_ACT_ALLOW);
  if (!seccomp)
    return flatpak_fail_error (error, FLATPAK_ERROR_SETUP_FAILED, _("Initialize seccomp failed"));

  if (arch != NULL)
    {
      uint32_t arch_id = 0;
      const uint32_t *extra_arches = NULL;

      if (strcmp (arch, "i386") == 0)
        {
          arch_id = SCMP_ARCH_X86;
        }
      else if (strcmp (arch, "x86_64") == 0)
        {
          arch_id = SCMP_ARCH_X86_64;
          extra_arches = seccomp_x86_64_extra_arches;
        }
      else if (strcmp (arch, "arm") == 0)
        {
          arch_id = SCMP_ARCH_ARM;
        }
#ifdef SCMP_ARCH_AARCH64
      else if (strcmp (arch, "aarch64") == 0)
        {
          arch_id = SCMP_ARCH_AARCH64;
          extra_arches = seccomp_aarch64_extra_arches;
        }
#endif

      /* We only really need to handle arches on multiarch systems.
       * If only one arch is supported the default is fine */
      if (arch_id != 0)
        {
          /* This *adds* the target arch, instead of replacing the
             native one. This is not ideal, because we'd like to only
             allow the target arch, but we can't really disallow the
             native arch at this point, because then bubblewrap
             couldn't continue running. */
          r = seccomp_arch_add (seccomp, arch_id);
          if (r < 0 && r != -EEXIST)
            return flatpak_fail_error (error, FLATPAK_ERROR_SETUP_FAILED, _("Failed to add architecture to seccomp filter"));

          if (multiarch && extra_arches != NULL)
            {
              for (i = 0; extra_arches[i] != 0; i++)
                {
                  r = seccomp_arch_add (seccomp, extra_arches[i]);
                  if (r < 0 && r != -EEXIST)
                    return flatpak_fail_error (error, FLATPAK_ERROR_SETUP_FAILED, _("Failed to add multiarch architecture to seccomp filter"));
                }
            }
        }
    }

  /* TODO: Should we filter the kernel keyring syscalls in some way?
   * We do want them to be used by desktop apps, but they could also perhaps
   * leak system stuff or secrets from other apps.
   */

  for (i = 0; i < G_N_ELEMENTS (syscall_blocklist); i++)
    {
      int scall = syscall_blocklist[i].scall;
      int errnum = syscall_blocklist[i].errnum;

      g_return_val_if_fail (errnum == EPERM || errnum == ENOSYS, FALSE);

      if (syscall_blocklist[i].arg)
        r = seccomp_rule_add (seccomp, SCMP_ACT_ERRNO (errnum), scall, 1, *syscall_blocklist[i].arg);
      else
        r = seccomp_rule_add (seccomp, SCMP_ACT_ERRNO (errnum), scall, 0);
      if (r < 0 && r == -EFAULT /* unknown syscall */)
        return flatpak_fail_error (error, FLATPAK_ERROR_SETUP_FAILED, _("Failed to block syscall %d"), scall);
    }

  if (!devel)
    {
      for (i = 0; i < G_N_ELEMENTS (syscall_nondevel_blocklist); i++)
        {
          int scall = syscall_nondevel_blocklist[i].scall;
          int errnum = syscall_nondevel_blocklist[i].errnum;

          g_return_val_if_fail (errnum == EPERM || errnum == ENOSYS, FALSE);

          if (syscall_nondevel_blocklist[i].arg)
            r = seccomp_rule_add (seccomp, SCMP_ACT_ERRNO (errnum), scall, 1, *syscall_nondevel_blocklist[i].arg);
          else
            r = seccomp_rule_add (seccomp, SCMP_ACT_ERRNO (errnum), scall, 0);

          if (r < 0 && r == -EFAULT /* unknown syscall */)
            return flatpak_fail_error (error, FLATPAK_ERROR_SETUP_FAILED, _("Failed to block syscall %d"), scall);
        }
    }

  /* Socket filtering doesn't work on e.g. i386, so ignore failures here
   * However, we need to user seccomp_rule_add_exact to avoid libseccomp doing
   * something else: https://github.com/seccomp/libseccomp/issues/8 */
  last_allowed_family = -1;
  for (i = 0; i < G_N_ELEMENTS (socket_family_allowlist); i++)
    {
      int family = socket_family_allowlist[i].family;
      int disallowed;

      if (socket_family_allowlist[i].flags_mask != 0 &&
          (socket_family_allowlist[i].flags_mask & run_flags) != socket_family_allowlist[i].flags_mask)
        continue;

      for (disallowed = last_allowed_family + 1; disallowed < family; disallowed++)
        {
          /* Blocklist the in-between valid families */
          seccomp_rule_add_exact (seccomp, SCMP_ACT_ERRNO (EAFNOSUPPORT), SCMP_SYS (socket), 1, SCMP_A0 (SCMP_CMP_EQ, disallowed));
        }
      last_allowed_family = family;
    }
  /* Blocklist the rest */
  seccomp_rule_add_exact (seccomp, SCMP_ACT_ERRNO (EAFNOSUPPORT), SCMP_SYS (socket), 1, SCMP_A0 (SCMP_CMP_GE, last_allowed_family + 1));

  if (!glnx_open_anonymous_tmpfile_full (O_RDWR | O_CLOEXEC, "/tmp", &seccomp_tmpf, error))
    return FALSE;

  if (seccomp_export_bpf (seccomp, seccomp_tmpf.fd) != 0)
    return flatpak_fail_error (error, FLATPAK_ERROR_SETUP_FAILED, _("Failed to export bpf"));

  lseek (seccomp_tmpf.fd, 0, SEEK_SET);

  flatpak_bwrap_add_args_data_fd (bwrap,
                                  "--seccomp", glnx_steal_fd (&seccomp_tmpf.fd), NULL);

  return TRUE;
}