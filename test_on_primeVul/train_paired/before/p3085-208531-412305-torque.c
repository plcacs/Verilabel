int tm_adopt(
    
  char  *id,
  int    adoptCmd,
  pid_t  pid)

  {
  int rc = TM_SUCCESS;
  int status, ret;
  pid_t sid;
  char *env;
  struct tcp_chan *chan = NULL;

  sid = getsid(pid);

  /* Must be the only call to call to tm and
     must only be called once */

  if (init_done) return TM_BADINIT;

  init_done = 1;

  /* Fabricate the tm state as best we can - not really needed */

  if ((tm_jobid = getenv("PBS_JOBID")) == NULL)
    tm_jobid = (char *)"ADOPT JOB";

  tm_jobid_len = strlen(tm_jobid);

  if ((tm_jobcookie = getenv("PBS_JOBCOOKIE")) == NULL)
    tm_jobcookie = (char *)"ADOPT COOKIE";

  tm_jobcookie_len = strlen(tm_jobcookie);

  /* We dont have the (right) node id or task id */
  tm_jobndid = 0;

  tm_jobtid = 0;

  /* Fallback is system default MOM port if not known */
  if ((env = getenv("PBS_MOMPORT")) == NULL || (tm_momport = atoi(env)) == 0)
    tm_momport = PBS_MANAGER_SERVICE_PORT;


  /* DJH 27 Feb 2002. two kinds of adoption now */
  if (adoptCmd != TM_ADOPT_ALTID && adoptCmd != TM_ADOPT_JOBID)
    return TM_EUNKNOWNCMD;

  if (startcom(adoptCmd, TM_NULL_EVENT, &chan) != DIS_SUCCESS)
    return TM_ESYSTEM;

  /* send session id */
  if (diswsi(chan, sid) != DIS_SUCCESS)
    {
    rc = TM_ENOTCONNECTED;
    goto tm_adopt_cleanup;
    }

  /* write the pid so the adopted process can be part of the cpuset if needed */
  if (diswsi(chan, pid) != DIS_SUCCESS)
    {
    rc =  TM_ENOTCONNECTED;
    goto tm_adopt_cleanup;
    }

  /* send job or alternative id */
  if (diswcs(chan, id, strlen(id)) != DIS_SUCCESS)
    {
    rc =  TM_ENOTCONNECTED;
    goto tm_adopt_cleanup;
    }

  DIS_tcp_wflush(chan);

  /* The mom should now attempt to adopt the task and will send back a
     status flag to indicate whether it was successful or not. */

  status = disrsi(chan, &ret);

  if (ret != DIS_SUCCESS)
    {
    rc = TM_ENOTCONNECTED;
    goto tm_adopt_cleanup;
    }

  /* Don't allow any more tm_* calls in this process. As well as
     closing an unused socket it also prevents any problems related to
     the fact that all adopted processes have a fake task id which
     might break the tm mechanism */
  tm_finalize();

  /* Since we're not using events, tm_finalize won't actually
     close the socket, so do it here. */
  if (local_conn > -1)
    {
    close(local_conn);
    local_conn = -1;
    }

  DIS_tcp_cleanup(chan);
  return (status == TM_OKAY ?

          TM_SUCCESS :
          TM_ENOTFOUND);

tm_adopt_cleanup:
  if (chan != NULL)
    DIS_tcp_cleanup(chan);
  return rc;
  }