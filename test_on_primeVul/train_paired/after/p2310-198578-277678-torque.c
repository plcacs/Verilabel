void svr_mailowner(

  job   *pjob,       /* I */
  int    mailpoint,  /* note, single character  */
  int    force,      /* if set to MAIL_FORCE, force mail delivery */
  char *text)      /* (optional) additional message text */

  {
  int         status = 0;
  int         numargs = 0;
  int         pipes[2];
  int         counter;
  pid_t       pid;
  char       *mailptr;
  char       *mailfrom = NULL;
  char        tmpBuf[LOG_BUF_SIZE];
  // We call sendmail with cmd_name + 2 arguments + # of mailto addresses + 1 for null
  char       *sendmail_args[100];
  char        mailto[1024];
  FILE       *stream;

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

  /* Close the rest of the open file descriptors */
  int numfds = sysconf(_SC_OPEN_MAX);
  while (--numfds > 0)
    close(numfds);
  
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
      int i;
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

  sendmail_args[numargs++] = (char *)SENDMAIL_CMD;
  sendmail_args[numargs++] = (char *)"-f";
  sendmail_args[numargs++] = (char *)mailfrom;

  /* Add the e-mail addresses to the command line */
  mailptr = strdup(mailto);
  sendmail_args[numargs++] = mailptr;
  for (counter=0; counter < (int)strlen(mailptr); counter++)
    {
    if (mailptr[counter] == ',')
      {
      mailptr[counter] = '\0';
      sendmail_args[numargs++] = mailptr + counter + 1;
      if (numargs >= 99)
        break;
      }
    }

  sendmail_args[numargs] = NULL;
  
  /* Create a pipe to talk to the sendmail process we are about to fork */
  if (pipe(pipes) == -1)
    {
    snprintf(tmpBuf, sizeof(tmpBuf), "Unable to pipes for sending e-mail\n");
    log_event(PBSEVENT_ERROR | PBSEVENT_ADMIN | PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid,
      tmpBuf);

    free(mailptr);
    exit(-1);
    }

  if ((pid=fork()) == -1)
    {
    snprintf(tmpBuf, sizeof(tmpBuf), "Unable to fork for sending e-mail\n");
    log_event(PBSEVENT_ERROR | PBSEVENT_ADMIN | PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid,
      tmpBuf);

    free(mailptr);
    close(pipes[0]);
    close(pipes[1]);
    exit(-1);
    }
  else if (pid == 0)
    {
    /* CHILD */

    /* Make stdin the read end of the pipe */
    dup2(pipes[0], 0);

    /* Close the rest of the open file descriptors */
    int numfds = sysconf(_SC_OPEN_MAX);
    while (--numfds > 0)
      close(numfds);

    execv(SENDMAIL_CMD, sendmail_args);
    /* This never returns, but if the execv fails the child should exit */
    exit(1);
    }
  else
    {
    /* This is the parent */

    /* Close the read end of the pipe */
    close(pipes[0]);

    /* Write the body to the pipe */
    stream = fdopen(pipes[1], "w");
    write_email(stream, pjob, mailto, mailpoint, text);

    fflush(stream);

    /* Close and wait for the command to finish */
    if (fclose(stream) != 0)
      {
      snprintf(tmpBuf,sizeof(tmpBuf),
        "Piping mail body to sendmail closed: errno %d:%s\n",
        errno, strerror(errno));

      log_event(PBSEVENT_ERROR | PBSEVENT_ADMIN | PBSEVENT_JOB,
        PBS_EVENTCLASS_JOB,
        pjob->ji_qs.ji_jobid,
        tmpBuf);
      }

    // we aren't going to block in order to find out whether or not sendmail worked 
    if ((waitpid(pid, &status, WNOHANG) != 0) &&
        (status != 0))
      {
      snprintf(tmpBuf,sizeof(tmpBuf),
        "Sendmail command returned %d. Mail may not have been sent\n",
        status);

      log_event(PBSEVENT_ERROR | PBSEVENT_ADMIN | PBSEVENT_JOB,
        PBS_EVENTCLASS_JOB,
        pjob->ji_qs.ji_jobid,
        tmpBuf);
      }

    // don't leave zombies
    while (waitpid(-1, &status, WNOHANG) != 0)
      {
      // zombie reaped, NO-OP
      }
      
    free(mailptr);
    exit(0);
    }
    
  /* NOT REACHED */

  exit(0);
  }  /* END svr_mailowner() */