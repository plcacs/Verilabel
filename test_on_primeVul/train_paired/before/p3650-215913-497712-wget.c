ftp_retrieve_glob (struct url *u, ccon *con, int action)
{
  struct fileinfo *f, *start;
  uerr_t res;

  con->cmd |= LEAVE_PENDING;

  res = ftp_get_listing (u, con, &start);
  if (res != RETROK)
    return res;
  /* First: weed out that do not conform the global rules given in
     opt.accepts and opt.rejects.  */
  if (opt.accepts || opt.rejects)
    {
      f = start;
      while (f)
        {
          if (f->type != FT_DIRECTORY && !acceptable (f->name))
            {
              logprintf (LOG_VERBOSE, _("Rejecting %s.\n"),
                         quote (f->name));
              f = delelement (f, &start);
            }
          else
            f = f->next;
        }
    }
  /* Remove all files with possible harmful names */
  f = start;
  while (f)
    {
      if (has_insecure_name_p (f->name))
        {
          logprintf (LOG_VERBOSE, _("Rejecting %s.\n"),
                     quote (f->name));
          f = delelement (f, &start);
        }
      else
        f = f->next;
    }
  /* Now weed out the files that do not match our globbing pattern.
     If we are dealing with a globbing pattern, that is.  */
  if (*u->file)
    {
      if (action == GLOB_GLOBALL)
        {
          int (*matcher) (const char *, const char *, int)
            = opt.ignore_case ? fnmatch_nocase : fnmatch;
          int matchres = 0;

          f = start;
          while (f)
            {
              matchres = matcher (u->file, f->name, 0);
              if (matchres == -1)
                {
                  logprintf (LOG_NOTQUIET, _("Error matching %s against %s: %s\n"),
                             u->file, quotearg_style (escape_quoting_style, f->name),
                             strerror (errno));
                  break;
                }
              if (matchres == FNM_NOMATCH)
                f = delelement (f, &start); /* delete the element from the list */
              else
                f = f->next;        /* leave the element in the list */
            }
          if (matchres == -1)
            {
              freefileinfo (start);
              return RETRBADPATTERN;
            }
        }
      else if (action == GLOB_GETONE)
        {
#ifdef __VMS
          /* 2009-09-09 SMS.
           * Odd-ball compiler ("HP C V7.3-009 on OpenVMS Alpha V7.3-2")
           * bug causes spurious %CC-E-BADCONDIT complaint with this
           * "?:" statement.  (Different linkage attributes for strcmp()
           * and strcasecmp().)  Converting to "if" changes the
           * complaint to %CC-W-PTRMISMATCH on "cmp = strcmp;".  Adding
           * the senseless type cast clears the complaint, and looks
           * harmless.
           */
          int (*cmp) (const char *, const char *)
            = opt.ignore_case ? strcasecmp : (int (*)())strcmp;
#else /* def __VMS */
          int (*cmp) (const char *, const char *)
            = opt.ignore_case ? strcasecmp : strcmp;
#endif /* def __VMS [else] */
          f = start;
          while (f)
            {
              if (0 != cmp(u->file, f->name))
                f = delelement (f, &start);
              else
                f = f->next;
            }
        }
    }
  if (start)
    {
      /* Just get everything.  */
      res = ftp_retrieve_list (u, start, con);
    }
  else
    {
      if (action == GLOB_GLOBALL)
        {
          /* No luck.  */
          /* #### This message SUCKS.  We should see what was the
             reason that nothing was retrieved.  */
          logprintf (LOG_VERBOSE, _("No matches on pattern %s.\n"),
                     quote (u->file));
        }
      else if (action == GLOB_GETONE) /* GLOB_GETONE or GLOB_GETALL */
        {
          /* Let's try retrieving it anyway.  */
          con->st |= ON_YOUR_OWN;
          res = ftp_loop_internal (u, NULL, con, NULL);
          return res;
        }

      /* If action == GLOB_GETALL, and the file list is empty, there's
         no point in trying to download anything or in complaining about
         it.  (An empty directory should not cause complaints.)
      */
    }
  freefileinfo (start);
  if (opt.quota && total_downloaded_bytes > opt.quota)
    return QUOTEXC;
  else
    return res;
}