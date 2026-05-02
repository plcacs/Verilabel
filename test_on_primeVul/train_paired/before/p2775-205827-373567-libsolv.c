testcase_read(Pool *pool, FILE *fp, const char *testcase, Queue *job, char **resultp, int *resultflagsp)
{
  Solver *solv;
  char *buf, *bufp;
  int bufl;
  char *testcasedir, *s;
  int l;
  char **pieces = 0;
  int npieces = 0;
  int prepared = 0;
  int closefp = !fp;
  int poolflagsreset = 0;
  int missing_features = 0;
  Id *genid = 0;
  int ngenid = 0;
  Queue autoinstq;

  if (resultp)
    *resultp = 0;
  if (resultflagsp)
    *resultflagsp = 0;
  if (!fp && !(fp = fopen(testcase, "r")))
    {
      pool_error(pool, 0, "testcase_read: could not open '%s'", testcase);
      return 0;
    }
  testcasedir = solv_strdup(testcase);
  s = strrchr(testcasedir, '/');
#ifdef _WIN32
  buf = strrchr(testcasedir, '\\');
  if (!s || (buf && buf > s))
    s = buf;
#endif
  if (s)
    s[1] = 0;
  else
    *testcasedir = 0;
  bufl = 1024;
  buf = solv_malloc(bufl);
  bufp = buf;
  solv = 0;
  queue_init(&autoinstq);
  for (;;)
    {
      if (bufp - buf + 16 > bufl)
	{
	  bufl = bufp - buf;
	  buf = solv_realloc(buf, bufl + 512);
	  bufp = buf + bufl;
          bufl += 512;
	}
      if (!fgets(bufp, bufl - (bufp - buf), fp))
	break;
      bufp = buf;
      l = strlen(buf);
      if (!l || buf[l - 1] != '\n')
	{
	  bufp += l;
	  continue;
	}
      buf[--l] = 0;
      s = buf;
      while (*s && (*s == ' ' || *s == '\t'))
	s++;
      if (!*s || *s == '#')
	continue;
      npieces = 0;
      /* split it in pieces */
      for (;;)
	{
	  while (*s == ' ' || *s == '\t')
	    s++;
	  if (!*s)
	    break;
	  pieces = solv_extend(pieces, npieces, 1, sizeof(*pieces), 7);
	  pieces[npieces++] = s;
	  while (*s && *s != ' ' && *s != '\t')
	    s++;
	  if (*s)
	    *s++ = 0;
	}
      pieces = solv_extend(pieces, npieces, 1, sizeof(*pieces), 7);
      pieces[npieces] = 0;
      if (!strcmp(pieces[0], "repo") && npieces >= 4)
	{
	  Repo *repo = repo_create(pool, pieces[1]);
	  FILE *rfp;
	  int prio, subprio;
	  const char *rdata;

	  prepared = 0;
          if (!poolflagsreset)
	    {
	      poolflagsreset = 1;
	      testcase_resetpoolflags(pool);	/* hmm */
	    }
	  if (sscanf(pieces[2], "%d.%d", &prio, &subprio) != 2)
	    {
	      subprio = 0;
	      prio = atoi(pieces[2]);
	    }
          repo->priority = prio;
          repo->subpriority = subprio;
	  if (strcmp(pieces[3], "empty") != 0)
	    {
	      const char *repotype = pool_tmpjoin(pool, pieces[3], 0, 0);	/* gets overwritten in <inline> case */
	      if (!strcmp(pieces[4], "<inline>"))
		{
		  char *idata = read_inline_file(fp, &buf, &bufp, &bufl);
		  rdata = "<inline>";
		  rfp = solv_fmemopen(idata, strlen(idata), "rf");
		}
	      else
		{
		  rdata = pool_tmpjoin(pool, testcasedir, pieces[4], 0);
		  rfp = solv_xfopen(rdata, "r");
		}
	      if (!rfp)
		{
		  pool_error(pool, 0, "testcase_read: could not open '%s'", rdata);
		}
	      else if (!strcmp(repotype, "testtags"))
		{
		  testcase_add_testtags(repo, rfp, 0);
		  fclose(rfp);
		}
	      else if (!strcmp(repotype, "solv"))
		{
		  repo_add_solv(repo, rfp, 0);
		  fclose(rfp);
		}
#if ENABLE_TESTCASE_HELIXREPO
	      else if (!strcmp(repotype, "helix"))
		{
		  repo_add_helix(repo, rfp, 0);
		  fclose(rfp);
		}
#endif
	      else
		{
		  fclose(rfp);
		  pool_error(pool, 0, "testcase_read: unknown repo type for repo '%s'", repo->name);
		}
	    }
	}
      else if (!strcmp(pieces[0], "system") && npieces >= 3)
	{
	  int i;

	  /* must set the disttype before the arch */
	  prepared = 0;
	  if (strcmp(pieces[2], "*") != 0)
	    {
	      char *dp = pieces[2];
	      while (dp && *dp)
		{
		  char *dpe = strchr(dp, ',');
		  if (dpe)
		    *dpe = 0;
		  for (i = 0; disttype2str[i].str != 0; i++)
		    if (!strcmp(disttype2str[i].str, dp))
		      break;
		  if (dpe)
		    *dpe++ = ',';
		  if (disttype2str[i].str)
		    {
#ifdef MULTI_SEMANTICS
		      if (pool->disttype != disttype2str[i].type)
		        pool_setdisttype(pool, disttype2str[i].type);
#endif
		      if (pool->disttype == disttype2str[i].type)
			break;
		    }
		  dp = dpe;
		}
	      if (!(dp && *dp))
		{
		  pool_error(pool, 0, "testcase_read: system: could not change disttype to '%s'", pieces[2]);
		  missing_features = 1;
		}
	    }
	  if (strcmp(pieces[1], "unset") == 0 || strcmp(pieces[1], "-") == 0)
	    pool_setarch(pool, 0);
	  else if (pieces[1][0] == ':')
	    pool_setarchpolicy(pool, pieces[1] + 1);
	  else
	    pool_setarch(pool, pieces[1]);
	  if (npieces > 3)
	    {
	      Repo *repo = testcase_str2repo(pool, pieces[3]);
	      if (!repo)
	        pool_error(pool, 0, "testcase_read: system: unknown repo '%s'", pieces[3]);
	      else
		pool_set_installed(pool, repo);
	    }
	}
      else if (!strcmp(pieces[0], "job") && npieces > 1)
	{
	  char *sp;
	  Id how, what;
	  if (prepared <= 0)
	    {
	      pool_addfileprovides(pool);
	      pool_createwhatprovides(pool);
	      prepared = 1;
	    }
	  if (npieces >= 3 && !strcmp(pieces[2], "selection"))
	    {
	      addselectionjob(pool, pieces + 1, npieces - 1, job, 0, 0);
	      continue;
	    }
	  if (npieces >= 4 && !strcmp(pieces[2], "selection_matchdeps"))
	    {
	      pieces[2] = pieces[1];
	      addselectionjob(pool, pieces + 2, npieces - 2, job, SELECTIONJOB_MATCHDEPS, pool_str2id(pool, pieces[3], 1));
	      continue;
	    }
	  if (npieces >= 4 && !strcmp(pieces[2], "selection_matchdepid"))
	    {
	      pieces[2] = pieces[1];
	      addselectionjob(pool, pieces + 2, npieces - 2, job, SELECTIONJOB_MATCHDEPID, pool_str2id(pool, pieces[3], 1));
	      continue;
	    }
	  if (npieces >= 4 && !strcmp(pieces[2], "selection_matchsolvable"))
	    {
	      pieces[2] = pieces[1];
	      addselectionjob(pool, pieces + 2, npieces - 2, job, SELECTIONJOB_MATCHSOLVABLE, pool_str2id(pool, pieces[3], 1));
	      continue;
	    }
	  /* rejoin */
	  for (sp = pieces[1]; sp < pieces[npieces - 1]; sp++)
	    if (*sp == 0)
	      *sp = ' ';
	  how = testcase_str2job(pool, pieces[1], &what);
	  if (how >= 0 && job)
	    queue_push2(job, how, what);
	}
      else if (!strcmp(pieces[0], "vendorclass") && npieces > 1)
	{
	  pool_addvendorclass(pool, (const char **)(pieces + 1));
	}
      else if (!strcmp(pieces[0], "namespace") && npieces > 1)
	{
	  int i = strlen(pieces[1]);
	  s = strchr(pieces[1], '(');
	  if (!s || pieces[1][i - 1] != ')')
	    {
	      pool_error(pool, 0, "testcase_read: bad namespace '%s'", pieces[1]);
	    }
	  else
	    {
	      Id name, evr, id;
	      Queue q;
	      queue_init(&q);
	      *s = 0;
	      pieces[1][i - 1] = 0;
	      name = pool_str2id(pool, pieces[1], 1);
	      evr = pool_str2id(pool, s + 1, 1);
	      *s = '(';
	      pieces[1][i - 1] = ')';
	      id = pool_rel2id(pool, name, evr, REL_NAMESPACE, 1);
	      for (i = 2; i < npieces; i++)
		queue_push(&q, testcase_str2solvid(pool, pieces[i]));
	      /* now do the callback */
	      if (prepared <= 0)
		{
		  pool_addfileprovides(pool);
		  pool_createwhatprovides(pool);
		  prepared = 1;
		}
	      pool->whatprovides_rel[GETRELID(id)] = pool_queuetowhatprovides(pool, &q);
	      queue_free(&q);
	    }
	}
      else if (!strcmp(pieces[0], "poolflags"))
        {
	  int i;
          if (!poolflagsreset)
	    {
	      poolflagsreset = 1;
	      testcase_resetpoolflags(pool);	/* hmm */
	    }
	  for (i = 1; i < npieces; i++)
	    testcase_setpoolflags(pool, pieces[i]);
        }
      else if (!strcmp(pieces[0], "solverflags") && npieces > 1)
        {
	  int i;
	  if (!solv)
	    {
	      solv = solver_create(pool);
	      testcase_resetsolverflags(solv);
	    }
	  for (i = 1; i < npieces; i++)
	    testcase_setsolverflags(solv, pieces[i]);
        }
      else if (!strcmp(pieces[0], "result") && npieces > 1)
	{
	  char *result = 0;
	  int resultflags = str2resultflags(pool, pieces[1]);
	  const char *rdata;
	  if (npieces > 2)
	    {
	      rdata = pool_tmpjoin(pool, testcasedir, pieces[2], 0);
	      if (!strcmp(pieces[2], "<inline>"))
		result = read_inline_file(fp, &buf, &bufp, &bufl);
	      else
		{
		  FILE *rfp = fopen(rdata, "r");
		  if (!rfp)
		    pool_error(pool, 0, "testcase_read: could not open '%s'", rdata);
		  else
		    {
		      result = read_file(rfp);
		      fclose(rfp);
		    }
		}
	    }
	  if (resultp)
	    *resultp = result;
	  else
	    solv_free(result);
	  if (resultflagsp)
	    *resultflagsp = resultflags;
	}
      else if (!strcmp(pieces[0], "nextjob"))
	{
	  if (npieces == 2 && resultflagsp && !strcmp(pieces[1], "reusesolver"))
	    *resultflagsp |= TESTCASE_RESULT_REUSE_SOLVER;
	  break;
	}
      else if (!strcmp(pieces[0], "disable") && npieces == 3)
	{
	  Id p, pp, jobsel, what = 0;
	  if (!prepared)
	    pool_createwhatprovides(pool);
	  prepared = -1;
	  if (!pool->considered)
	    {
	      pool->considered = solv_calloc(1, sizeof(Map));
	      map_init(pool->considered, pool->nsolvables);
	      map_setall(pool->considered);
	    }
	  jobsel = testcase_str2jobsel(pool, "disable", pieces + 1, npieces - 1, &what);
	  if (jobsel < 0)
	    continue;
	  if (jobsel == SOLVER_SOLVABLE_ALL)
	    map_empty(pool->considered);
	  else if (jobsel == SOLVER_SOLVABLE_REPO)
	    {
	      Repo *repo = pool_id2repo(pool, what);
	      Solvable *s;
	      FOR_REPO_SOLVABLES(repo, p, s)
		MAPCLR(pool->considered, p);
	    }
	  FOR_JOB_SELECT(p, pp, jobsel, what)
	    MAPCLR(pool->considered, p);
	}
      else if (!strcmp(pieces[0], "feature"))
	{
	  int i, j;
	  for (i = 1; i < npieces; i++)
	    {
	      for (j = 0; features[j]; j++)
		if (!strcmp(pieces[i], features[j]))
		  break;
	      if (!features[j])
		{
		  pool_error(pool, 0, "testcase_read: missing feature '%s'", pieces[i]);
		  missing_features++;
		}
	    }
	  if (missing_features)
	    break;
	}
      else if (!strcmp(pieces[0], "genid") && npieces > 1)
	{
	  Id id;
	  /* rejoin */
	  if (npieces > 2)
	    {
	      char *sp;
	      for (sp = pieces[2]; sp < pieces[npieces - 1]; sp++)
	        if (*sp == 0)
	          *sp = ' ';
	    }
	  genid = solv_extend(genid, ngenid, 1, sizeof(*genid), 7);
	  if (!strcmp(pieces[1], "op") && npieces > 2)
	    {
	      struct oplist *op;
	      for (op = oplist; op->flags; op++)
		if (!strncmp(pieces[2], op->opname, strlen(op->opname)))
		  break;
	      if (!op->flags)
		{
		  pool_error(pool, 0, "testcase_read: genid: unknown op '%s'", pieces[2]);
		  break;
		}
	      if (ngenid < 2)
		{
		  pool_error(pool, 0, "testcase_read: genid: out of stack");
		  break;
		}
	      ngenid -= 2;
	      id = pool_rel2id(pool, genid[ngenid] , genid[ngenid + 1], op->flags, 1);
	    }
	  else if (!strcmp(pieces[1], "lit"))
	    id = pool_str2id(pool, npieces > 2 ? pieces[2] : "", 1);
	  else if (!strcmp(pieces[1], "null"))
	    id = 0;
	  else if (!strcmp(pieces[1], "dep"))
	    id = testcase_str2dep(pool, pieces[2]);
	  else
	    {
	      pool_error(pool, 0, "testcase_read: genid: unknown command '%s'", pieces[1]);
	      break;
	    }
	  genid[ngenid++] = id;
	}
      else if (!strcmp(pieces[0], "autoinst") && npieces > 2)
	{
	  if (strcmp(pieces[1], "name"))
	    {
	      pool_error(pool, 0, "testcase_read: autoinst: illegal mode");
	      break;
	    }
	  queue_push(&autoinstq, pool_str2id(pool, pieces[2], 1));
	}
      else if (!strcmp(pieces[0], "evrcmp") && npieces == 3)
	{
	  Id evr1 = pool_str2id(pool, pieces[1], 1);
	  Id evr2 = pool_str2id(pool, pieces[2], 1);
	  int r = pool_evrcmp(pool, evr1, evr2, EVRCMP_COMPARE);
	  r = r < 0 ? REL_LT : r > 0 ? REL_GT : REL_EQ;
	  queue_push2(job, SOLVER_NOOP | SOLVER_SOLVABLE_PROVIDES, pool_rel2id(pool, evr1, evr2, r, 1));
	}
      else
	{
	  pool_error(pool, 0, "testcase_read: cannot parse command '%s'", pieces[0]);
	}
    }
  while (job && ngenid > 0)
    queue_push2(job, SOLVER_NOOP | SOLVER_SOLVABLE_PROVIDES, genid[--ngenid]);
  if (autoinstq.count)
    pool_add_userinstalled_jobs(pool, &autoinstq, job, GET_USERINSTALLED_NAMES | GET_USERINSTALLED_INVERTED);
  queue_free(&autoinstq);
  genid = solv_free(genid);
  buf = solv_free(buf);
  pieces = solv_free(pieces);
  solv_free(testcasedir);
  if (!prepared)
    {
      pool_addfileprovides(pool);
      pool_createwhatprovides(pool);
    }
  if (!solv)
    {
      solv = solver_create(pool);
      testcase_resetsolverflags(solv);
    }
  if (closefp)
    fclose(fp);
  if (missing_features)
    {
      solver_free(solv);
      solv = 0;
      if (resultflagsp)
	*resultflagsp = 77;	/* hack for testsolv */
    }
  return solv;
}