void svr_mailowner(

  job   *pjob,       /* I */
  int   mailpoint,  /* note, single character  */
  int    force,      /* if set to MAIL_FORCE, force mail delivery */
  char *text)      /* (optional) additional message text */

  {
  char *cmdbuf;
  int    i;
  char *mailfrom;
  char  mailto[1024];
  char *bodyfmt, *subjectfmt;
  char bodyfmtbuf[1024];
  FILE *outmail;

  struct array_strings *pas;

  if ((server.sv_attr[SRV_ATR_MailDomain].at_flags & ATR_VFLAG_SET) &&
      (server.sv_attr[SRV_ATR_MailDomain].at_val.at_str != NULL) &&
      (!strcasecmp("never", server.sv_attr[SRV_ATR_MailDomain].at_val.at_str)))
    {
    /* never send user mail under any conditions */
    if (LOGLEVEL >= 3) 
      {
      log_event(PBSEVENT_ERROR | PBSEVENT_ADMIN | PBSEVENT_JOB,
        PBS_EVENTCLASS_JOB,
        pjob->ji_qs.ji_jobid,
        "Not sending email: Mail domain set to 'never'\n");
      }

    return;
    }

  if (LOGLEVEL >= 3)
    {
    char tmpBuf[LOG_BUF_SIZE];

    snprintf(tmpBuf, LOG_BUF_SIZE, "preparing to send '%c' mail for job %s to %s (%.64s)\n",
             (char)mailpoint,
             pjob->ji_qs.ji_jobid,
             pjob->ji_wattr[JOB_ATR_job_owner].at_val.at_str,
             (text != NULL) ? text : "---");

    log_event(
      PBSEVENT_ERROR | PBSEVENT_ADMIN | PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid,
      tmpBuf);
    }

  /*
   * if force is true, force the mail out regardless of mailpoint
   * unless server no_mail_force attribute is set to true
   */

  if ((force != MAIL_FORCE) ||
    (server.sv_attr[(int)SRV_ATR_NoMailForce].at_val.at_long == TRUE))
    {

    if (pjob->ji_wattr[JOB_ATR_mailpnts].at_flags & ATR_VFLAG_SET)
      {
      if (*(pjob->ji_wattr[JOB_ATR_mailpnts].at_val.at_str) ==  MAIL_NONE)
        {
        /* do not send mail. No mail requested on job */
        log_event(PBSEVENT_JOB,
                  PBS_EVENTCLASS_JOB,
                  pjob->ji_qs.ji_jobid,
                  "Not sending email: job requested no e-mail");
        return;
        }
      /* see if user specified mail of this type */
      if (strchr(
            pjob->ji_wattr[JOB_ATR_mailpnts].at_val.at_str,
            mailpoint) == NULL)
        {
        /* do not send mail */
        log_event(PBSEVENT_ERROR | PBSEVENT_ADMIN | PBSEVENT_JOB,
          PBS_EVENTCLASS_JOB,
          pjob->ji_qs.ji_jobid,
          "Not sending email: User does not want mail of this type.\n");

        return;
        }
      }
    else if (mailpoint != MAIL_ABORT) /* not set, default to abort */
      {
      log_event(PBSEVENT_ERROR | PBSEVENT_ADMIN | PBSEVENT_JOB,
        PBS_EVENTCLASS_JOB,
        pjob->ji_qs.ji_jobid,
        "Not sending email: Default mailpoint does not include this type.\n");

      return;
      }
    }

  /*
   * ok, now we will fork a process to do the mailing to not
   * hold up the server's other work.
   */

  if (fork())
    {
    return;  /* its all up to the child now */
    }

  /*
   * From here on, we are a child process of the server.
   * Fix up file descriptors and signal handlers.
   */

  rpp_terminate();

  net_close(-1);

  /* Who is mail from, if SRV_ATR_mailfrom not set use default */

  if ((mailfrom = server.sv_attr[SRV_ATR_mailfrom].at_val.at_str) == NULL)
    {
    if (LOGLEVEL >= 5)
      {
      char tmpBuf[LOG_BUF_SIZE];

      snprintf(tmpBuf,sizeof(tmpBuf),
        "Updated mailto from user list: '%s'\n",
        mailto);
      log_event(PBSEVENT_ERROR | PBSEVENT_ADMIN | PBSEVENT_JOB,
        PBS_EVENTCLASS_JOB,
        pjob->ji_qs.ji_jobid,
        tmpBuf);
      }
    mailfrom = PBS_DEFAULT_MAIL;
    }

  /* Who does the mail go to?  If mail-list, them; else owner */

  *mailto = '\0';

  if (pjob->ji_wattr[JOB_ATR_mailuser].at_flags & ATR_VFLAG_SET)
    {
    /* has mail user list, send to them rather than owner */

    pas = pjob->ji_wattr[JOB_ATR_mailuser].at_val.at_arst;

    if (pas != NULL)
      {
      for (i = 0;i < pas->as_usedptr;i++)
        {
        if ((strlen(mailto) + strlen(pas->as_string[i]) + 2) < sizeof(mailto))
          {
          strcat(mailto, pas->as_string[i]);
          strcat(mailto, " ");
          }
        }
      }
    }
  else
    {
    /* no mail user list, just send to owner */

    if ((server.sv_attr[SRV_ATR_MailDomain].at_flags & ATR_VFLAG_SET) &&
        (server.sv_attr[SRV_ATR_MailDomain].at_val.at_str != NULL))
      {
      strcpy(mailto, pjob->ji_wattr[JOB_ATR_euser].at_val.at_str);
      strcat(mailto, "@");
      strcat(mailto, server.sv_attr[SRV_ATR_MailDomain].at_val.at_str);

      if (LOGLEVEL >= 5) 
        {
        char tmpBuf[LOG_BUF_SIZE];

        snprintf(tmpBuf,sizeof(tmpBuf),
          "Updated mailto from job owner and mail domain: '%s'\n",
          mailto);
        log_event(PBSEVENT_ERROR | PBSEVENT_ADMIN | PBSEVENT_JOB,
          PBS_EVENTCLASS_JOB,
          pjob->ji_qs.ji_jobid,
          tmpBuf);
        }
      }
    else
      {
#ifdef TMAILDOMAIN
      strcpy(mailto, pjob->ji_wattr[JOB_ATR_euser].at_val.at_str);
      strcat(mailto, "@");
      strcat(mailto, TMAILDOMAIN);
#else /* TMAILDOMAIN */
      strcpy(mailto, pjob->ji_wattr[JOB_ATR_job_owner].at_val.at_str);
#endif /* TMAILDOMAIN */

      if (LOGLEVEL >= 5)
        {
        char tmpBuf[LOG_BUF_SIZE];

        snprintf(tmpBuf,sizeof(tmpBuf),
          "Updated mailto from job owner: '%s'\n",
          mailto);
        log_event(PBSEVENT_ERROR | PBSEVENT_ADMIN | PBSEVENT_JOB,
          PBS_EVENTCLASS_JOB,
          pjob->ji_qs.ji_jobid,
          tmpBuf);
        }
      }
    }

  /* mail subject line formating statement */

  if ((server.sv_attr[SRV_ATR_MailSubjectFmt].at_flags & ATR_VFLAG_SET) &&
      (server.sv_attr[SRV_ATR_MailSubjectFmt].at_val.at_str != NULL))
    {
    subjectfmt = server.sv_attr[SRV_ATR_MailSubjectFmt].at_val.at_str;
    }
  else
    {
    subjectfmt = "PBS JOB %i";
    }

  /* mail body formating statement */

  if ((server.sv_attr[SRV_ATR_MailBodyFmt].at_flags & ATR_VFLAG_SET) &&
      (server.sv_attr[SRV_ATR_MailBodyFmt].at_val.at_str != NULL))
    {
    bodyfmt = server.sv_attr[SRV_ATR_MailBodyFmt].at_val.at_str;
    }
  else
    {
    bodyfmt =  strcpy(bodyfmtbuf, "PBS Job Id: %i\n"
                                  "Job Name:   %j\n");
    if (pjob->ji_wattr[JOB_ATR_exec_host].at_flags & ATR_VFLAG_SET)
      {
      strcat(bodyfmt, "Exec host:  %h\n");
      }

    strcat(bodyfmt, "%m\n");

    if (text != NULL)
      {
      strcat(bodyfmt, "%d\n");
      }
    }
  /* setup sendmail command line with -f from_whom */

  i = strlen(SENDMAIL_CMD) + strlen(mailfrom) + strlen(mailto) + 6;

  if ((cmdbuf = malloc(i)) == NULL)
    {
    char tmpBuf[LOG_BUF_SIZE];

    snprintf(tmpBuf,sizeof(tmpBuf),
      "Unable to popen() command '%s' for writing: '%s' (error %d)\n",
      cmdbuf,
      strerror(errno),
      errno);
    log_event(PBSEVENT_ERROR | PBSEVENT_ADMIN | PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid,
      tmpBuf);

    exit(1);
    }

  sprintf(cmdbuf, "%s -f %s %s",

          SENDMAIL_CMD,
          mailfrom,
          mailto);

  outmail = (FILE *)popen(cmdbuf, "w");

  if (outmail == NULL)
    {
    char tmpBuf[LOG_BUF_SIZE];

    snprintf(tmpBuf,sizeof(tmpBuf),
      "Unable to popen() command '%s' for writing: '%s' (error %d)\n",
      cmdbuf,
      strerror(errno),
      errno);
    log_event(PBSEVENT_ERROR | PBSEVENT_ADMIN | PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid,
      tmpBuf);

    exit(1);
    }

  /* Pipe in mail headers: To: and Subject: */

  fprintf(outmail, "To: %s\n",
          mailto);

  fprintf(outmail, "Subject: ");
  svr_format_job(outmail, pjob, subjectfmt, mailpoint, text);
  fprintf(outmail, "\n");

  /* Set "Precedence: bulk" to avoid vacation messages, etc */

  fprintf(outmail, "Precedence: bulk\n\n");

  /* Now pipe in the email body */
  svr_format_job(outmail, pjob, bodyfmt, mailpoint, text);

  errno = 0;
  if ((i = pclose(outmail)) != 0)
    {
    char tmpBuf[LOG_BUF_SIZE];

    snprintf(tmpBuf,sizeof(tmpBuf),
      "Email '%c' to %s failed: Child process '%s' %s %d (errno %d:%s)\n",
      mailpoint,
      mailto,
      cmdbuf,
      ((WIFEXITED(i)) ? ("returned") : ((WIFSIGNALED(i)) ? ("killed by signal") : ("croaked"))),
      ((WIFEXITED(i)) ? (WEXITSTATUS(i)) : ((WIFSIGNALED(i)) ? (WTERMSIG(i)) : (i))),
      errno,
      strerror(errno));
    log_event(PBSEVENT_ERROR | PBSEVENT_ADMIN | PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid,
      tmpBuf);
    }
  else if (LOGLEVEL >= 4)
    {
    log_event(PBSEVENT_ERROR | PBSEVENT_ADMIN | PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid,
      "Email sent successfully\n");
    }

  exit(0);

  /*NOTREACHED*/

  return;
  }  /* END svr_mailowner() */