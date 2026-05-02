dbd_st_prepare(
  SV *sth,
  imp_sth_t *imp_sth,
  char *statement,
  SV *attribs)
{
  int i;
  SV **svp;
  dTHX;
#if MYSQL_VERSION_ID >= SERVER_PREPARE_VERSION
#if MYSQL_VERSION_ID < CALL_PLACEHOLDER_VERSION
  char *str_ptr, *str_last_ptr;
#if MYSQL_VERSION_ID < LIMIT_PLACEHOLDER_VERSION
  int limit_flag=0;
#endif
#endif
  int col_type, prepare_retval;
  MYSQL_BIND *bind, *bind_end;
  imp_sth_phb_t *fbind;
#endif
  D_imp_xxh(sth);
  D_imp_dbh_from_sth;

  if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
    PerlIO_printf(DBIc_LOGPIO(imp_xxh),
                 "\t-> dbd_st_prepare MYSQL_VERSION_ID %d, SQL statement: %s\n",
                  MYSQL_VERSION_ID, statement);

#if MYSQL_VERSION_ID >= SERVER_PREPARE_VERSION
 /* Set default value of 'mysql_server_prepare' attribute for sth from dbh */
  imp_sth->use_server_side_prepare= imp_dbh->use_server_side_prepare;
  if (attribs)
  {
    svp= DBD_ATTRIB_GET_SVP(attribs, "mysql_server_prepare", 20);
    imp_sth->use_server_side_prepare = (svp) ?
      SvTRUE(*svp) : imp_dbh->use_server_side_prepare;

    svp = DBD_ATTRIB_GET_SVP(attribs, "async", 5);

    if(svp && SvTRUE(*svp)) {
#if MYSQL_ASYNC
        imp_sth->is_async = TRUE;
        imp_sth->use_server_side_prepare = FALSE;
#else
        do_error(sth, 2000,
                 "Async support was not built into this version of DBD::mysql", "HY000");
        return 0;
#endif
    }
  }

  imp_sth->fetch_done= 0;
#endif

  imp_sth->done_desc= 0;
  imp_sth->result= NULL;
  imp_sth->currow= 0;

  /* Set default value of 'mysql_use_result' attribute for sth from dbh */
  svp= DBD_ATTRIB_GET_SVP(attribs, "mysql_use_result", 16);
  imp_sth->use_mysql_use_result= svp ?
    SvTRUE(*svp) : imp_dbh->use_mysql_use_result;

  for (i= 0; i < AV_ATTRIB_LAST; i++)
    imp_sth->av_attr[i]= Nullav;

  /*
     Clean-up previous result set(s) for sth to prevent
     'Commands out of sync' error 
  */
  mysql_st_free_result_sets(sth, imp_sth);

#if MYSQL_VERSION_ID >= SERVER_PREPARE_VERSION && MYSQL_VERSION_ID < CALL_PLACEHOLDER_VERSION
  if (imp_sth->use_server_side_prepare)
  {
    if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
      PerlIO_printf(DBIc_LOGPIO(imp_xxh),
                    "\t\tuse_server_side_prepare set, check restrictions\n");
    /*
      This code is here because placeholder support is not implemented for
      statements with :-
      1. LIMIT < 5.0.7
      2. CALL < 5.5.3 (Added support for out & inout parameters)
      In these cases we have to disable server side prepared statements
      NOTE: These checks could cause a false positive on statements which
      include columns / table names that match "call " or " limit "
    */ 
    if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
      PerlIO_printf(DBIc_LOGPIO(imp_xxh),
#if MYSQL_VERSION_ID < LIMIT_PLACEHOLDER_VERSION
                    "\t\tneed to test for LIMIT & CALL\n");
#else
                    "\t\tneed to test for restrictions\n");
#endif
    str_last_ptr = statement + strlen(statement);
    for (str_ptr= statement; str_ptr < str_last_ptr; str_ptr++)
    {
#if MYSQL_VERSION_ID < LIMIT_PLACEHOLDER_VERSION
      /*
        Place holders not supported in LIMIT's
      */
      if (limit_flag)
      {
        if (*str_ptr == '?')
        {
          if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
            PerlIO_printf(DBIc_LOGPIO(imp_xxh),
                    "\t\tLIMIT and ? found, set to use_server_side_prepare=0\n");
          /* ... then we do not want to try server side prepare (use emulation) */
          imp_sth->use_server_side_prepare= 0;
          break;
        }
      }
      else if (str_ptr < str_last_ptr - 6 &&
          isspace(*(str_ptr + 0)) &&
          tolower(*(str_ptr + 1)) == 'l' &&
          tolower(*(str_ptr + 2)) == 'i' &&
          tolower(*(str_ptr + 3)) == 'm' &&
          tolower(*(str_ptr + 4)) == 'i' &&
          tolower(*(str_ptr + 5)) == 't' &&
          isspace(*(str_ptr + 6)))
      {
        if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
          PerlIO_printf(DBIc_LOGPIO(imp_xxh), "LIMIT set limit flag to 1\n");
        limit_flag= 1;
      }
#endif
      /*
        Place holders not supported in CALL's
      */
      if (str_ptr < str_last_ptr - 4 &&
           tolower(*(str_ptr + 0)) == 'c' &&
           tolower(*(str_ptr + 1)) == 'a' &&
           tolower(*(str_ptr + 2)) == 'l' &&
           tolower(*(str_ptr + 3)) == 'l' &&
           isspace(*(str_ptr + 4)))
      {
        if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
          PerlIO_printf(DBIc_LOGPIO(imp_xxh), "Disable PS mode for CALL()\n");
        imp_sth->use_server_side_prepare= 0;
        break;
      }
    }
  }
#endif

#if MYSQL_VERSION_ID >= SERVER_PREPARE_VERSION
  if (imp_sth->use_server_side_prepare)
  {
    if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
      PerlIO_printf(DBIc_LOGPIO(imp_xxh),
                    "\t\tuse_server_side_prepare set\n");
    /* do we really need this? If we do, we should return, not just continue */
    if (imp_sth->stmt)
      fprintf(stderr,
              "ERROR: Trying to prepare new stmt while we have \
              already not closed one \n");

    imp_sth->stmt= mysql_stmt_init(imp_dbh->pmysql);

    if (! imp_sth->stmt)
    {
      if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
        PerlIO_printf(DBIc_LOGPIO(imp_xxh),
                      "\t\tERROR: Unable to return MYSQL_STMT structure \
                      from mysql_stmt_init(): ERROR NO: %d ERROR MSG:%s\n",
                      mysql_errno(imp_dbh->pmysql),
                      mysql_error(imp_dbh->pmysql));
    }

    prepare_retval= mysql_stmt_prepare(imp_sth->stmt,
                                       statement,
                                       strlen(statement));
    if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
        PerlIO_printf(DBIc_LOGPIO(imp_xxh),
                      "\t\tmysql_stmt_prepare returned %d\n",
                      prepare_retval);

    if (prepare_retval)
    {
      if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
        PerlIO_printf(DBIc_LOGPIO(imp_xxh),
                      "\t\tmysql_stmt_prepare %d %s\n",
                      mysql_stmt_errno(imp_sth->stmt),
                      mysql_stmt_error(imp_sth->stmt));

      /* For commands that are not supported by server side prepared statement
         mechanism lets try to pass them through regular API */
      if (mysql_stmt_errno(imp_sth->stmt) == ER_UNSUPPORTED_PS)
      {
        if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
          PerlIO_printf(DBIc_LOGPIO(imp_xxh),
                    "\t\tSETTING imp_sth->use_server_side_prepare to 0\n");
        imp_sth->use_server_side_prepare= 0;
      }
      else
      {
        do_error(sth, mysql_stmt_errno(imp_sth->stmt),
                 mysql_stmt_error(imp_sth->stmt),
                mysql_sqlstate(imp_dbh->pmysql));
        mysql_stmt_close(imp_sth->stmt);
        imp_sth->stmt= NULL;
        return FALSE;
      }
    }
    else
    {
      DBIc_NUM_PARAMS(imp_sth)= mysql_stmt_param_count(imp_sth->stmt);
      /* mysql_stmt_param_count */

      if (DBIc_NUM_PARAMS(imp_sth) > 0)
      {
        int has_statement_fields= imp_sth->stmt->fields != 0;
        /* Allocate memory for bind variables */
        imp_sth->bind=            alloc_bind(DBIc_NUM_PARAMS(imp_sth));
        imp_sth->fbind=           alloc_fbind(DBIc_NUM_PARAMS(imp_sth));
        imp_sth->has_been_bound=  0;

        /* Initialize ph variables with  NULL values */
        for (i= 0,
             bind=      imp_sth->bind,
             fbind=     imp_sth->fbind,
             bind_end=  bind+DBIc_NUM_PARAMS(imp_sth);
             bind < bind_end ;
             bind++, fbind++, i++ )
        {
          /*
            if this statement has a result set, field types will be
            correctly identified. If there is no result set, such as
            with an INSERT, fields will not be defined, and all buffer_type
            will default to MYSQL_TYPE_VAR_STRING
          */
          col_type= (has_statement_fields ?
                     imp_sth->stmt->fields[i].type : MYSQL_TYPE_STRING);

          bind->buffer_type=  mysql_to_perl_type(col_type);

          if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
            PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\t\tmysql_to_perl_type returned %d\n", col_type);

          bind->buffer=       NULL;
          bind->length=       &(fbind->length);
          bind->is_null=      (char*) &(fbind->is_null);
          fbind->is_null=     1;
          fbind->length=      0;
        }
      }
    }
  }
#endif

#if MYSQL_VERSION_ID >= SERVER_PREPARE_VERSION
  /* Count the number of parameters (driver, vs server-side) */
  if (imp_sth->use_server_side_prepare == 0)
    DBIc_NUM_PARAMS(imp_sth) = count_params((imp_xxh_t *)imp_dbh, aTHX_ statement,
                                            imp_dbh->bind_comment_placeholders);
#else
  DBIc_NUM_PARAMS(imp_sth) = count_params((imp_xxh_t *)imp_dbh, aTHX_ statement,
                                          imp_dbh->bind_comment_placeholders);
#endif

  /* Allocate memory for parameters */
  imp_sth->params= alloc_param(DBIc_NUM_PARAMS(imp_sth));
  DBIc_IMPSET_on(imp_sth);

  if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
    PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\t<- dbd_st_prepare\n");
  return 1;
}