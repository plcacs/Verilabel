dbd_st_fetch(SV *sth, imp_sth_t* imp_sth)
{
  dTHX;
  int num_fields, ChopBlanks, i, rc;
  unsigned long *lengths;
  AV *av;
  int av_length, av_readonly;
  MYSQL_ROW cols;
  D_imp_dbh_from_sth;
  MYSQL* svsock= imp_dbh->pmysql;
  imp_sth_fbh_t *fbh;
  D_imp_xxh(sth);
#if MYSQL_VERSION_ID >=SERVER_PREPARE_VERSION
  MYSQL_BIND *buffer;
#endif
  MYSQL_FIELD *fields;
  if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
    PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\t-> dbd_st_fetch\n");

#if MYSQL_ASYNC
  if(imp_dbh->async_query_in_flight) {
      if(mysql_db_async_result(sth, &imp_sth->result) <= 0) {
        return Nullav;
      }
  }
#endif

#if MYSQL_VERSION_ID >=SERVER_PREPARE_VERSION
  if (imp_sth->use_server_side_prepare)
  {
    if (!DBIc_ACTIVE(imp_sth) )
    {
      do_error(sth, JW_ERR_SEQUENCE, "no statement executing\n",NULL);
      return Nullav;
    }

    if (imp_sth->fetch_done)
    {
      do_error(sth, JW_ERR_SEQUENCE, "fetch() but fetch already done",NULL);
      return Nullav;
    }

    if (!imp_sth->done_desc)
    {
      if (!dbd_describe(sth, imp_sth))
      {
        do_error(sth, JW_ERR_SEQUENCE, "Error while describe result set.",
                 NULL);
        return Nullav;
      }
    }
  }
#endif

  ChopBlanks = DBIc_is(imp_sth, DBIcf_ChopBlanks);

  if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
    PerlIO_printf(DBIc_LOGPIO(imp_xxh),
                  "\t\tdbd_st_fetch for %p, chopblanks %d\n",
                  sth, ChopBlanks);

  if (!imp_sth->result)
  {
    do_error(sth, JW_ERR_SEQUENCE, "fetch() without execute()" ,NULL);
    return Nullav;
  }

  /* fix from 2.9008 */
  imp_dbh->pmysql->net.last_errno = 0;

#if MYSQL_VERSION_ID >=SERVER_PREPARE_VERSION
  if (imp_sth->use_server_side_prepare)
  {
    if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
      PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\t\tdbd_st_fetch calling mysql_fetch\n");

    if ((rc= mysql_stmt_fetch(imp_sth->stmt)))
    {
      if (rc == 1)
        do_error(sth, mysql_stmt_errno(imp_sth->stmt),
                 mysql_stmt_error(imp_sth->stmt),
                mysql_stmt_sqlstate(imp_sth->stmt));

#if MYSQL_VERSION_ID >= MYSQL_VERSION_5_0 
      if (rc == MYSQL_DATA_TRUNCATED) {
        if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
          PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\t\tdbd_st_fetch data truncated\n");
        goto process;
      }
#endif

      if (rc == MYSQL_NO_DATA)
      {
        /* Update row_num to affected_rows value */
        imp_sth->row_num= mysql_stmt_affected_rows(imp_sth->stmt);
        imp_sth->fetch_done=1;
        if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
          PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\t\tdbd_st_fetch no data\n");
      }

      dbd_st_finish(sth, imp_sth);

      return Nullav;
    }

process:
    imp_sth->currow++;

    av= DBIc_DBISTATE(imp_sth)->get_fbav(imp_sth);
    num_fields=mysql_stmt_field_count(imp_sth->stmt);
    if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
      PerlIO_printf(DBIc_LOGPIO(imp_xxh),
                    "\t\tdbd_st_fetch called mysql_fetch, rc %d num_fields %d\n",
                    rc, num_fields);

    for (
         buffer= imp_sth->buffer,
         fbh= imp_sth->fbh,
         i= 0;
         i < num_fields;
         i++,
         fbh++,
         buffer++
        )
    {
      SV *sv= AvARRAY(av)[i]; /* Note: we (re)use the SV in the AV	*/
      STRLEN len;

      /* This is wrong, null is not being set correctly
       * This is not the way to determine length (this would break blobs!)
       */
      if (fbh->is_null)
        (void) SvOK_off(sv);  /*  Field is NULL, return undef  */
      else
      {
        /* In case of BLOB/TEXT fields we allocate only 8192 bytes
           in dbd_describe() for data. Here we know real size of field
           so we should increase buffer size and refetch column value
        */
        if (fbh->length > buffer->buffer_length || fbh->error)
        {
          if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
            PerlIO_printf(DBIc_LOGPIO(imp_xxh),
              "\t\tRefetch BLOB/TEXT column: %d, length: %lu, error: %d\n",
              i, fbh->length, fbh->error);

          Renew(fbh->data, fbh->length, char);
          buffer->buffer_length= fbh->length;
          buffer->buffer= (char *) fbh->data;
          imp_sth->stmt->bind[i].buffer_length = fbh->length;
          imp_sth->stmt->bind[i].buffer = (char *)fbh->data;

          if (DBIc_TRACE_LEVEL(imp_xxh) >= 2) {
            int j;
            int m = MIN(*buffer->length, buffer->buffer_length);
            char *ptr = (char*)buffer->buffer;
            PerlIO_printf(DBIc_LOGPIO(imp_xxh),"\t\tbefore buffer->buffer: ");
            for (j = 0; j < m; j++) {
              PerlIO_printf(DBIc_LOGPIO(imp_xxh), "%c", *ptr++);
            }
            PerlIO_printf(DBIc_LOGPIO(imp_xxh),"\n");
          }

          /*TODO: Use offset instead of 0 to fetch only remain part of data*/
          if (mysql_stmt_fetch_column(imp_sth->stmt, buffer , i, 0))
            do_error(sth, mysql_stmt_errno(imp_sth->stmt),
                     mysql_stmt_error(imp_sth->stmt),
                     mysql_stmt_sqlstate(imp_sth->stmt));

          if (DBIc_TRACE_LEVEL(imp_xxh) >= 2) {
            int j;
            int m = MIN(*buffer->length, buffer->buffer_length);
            char *ptr = (char*)buffer->buffer;
            PerlIO_printf(DBIc_LOGPIO(imp_xxh),"\t\tafter buffer->buffer: ");
            for (j = 0; j < m; j++) {
              PerlIO_printf(DBIc_LOGPIO(imp_xxh), "%c", *ptr++);
            }
            PerlIO_printf(DBIc_LOGPIO(imp_xxh),"\n");
          }
        }

        /* This does look a lot like Georg's PHP driver doesn't it?  --Brian */
        /* Credit due to Georg - mysqli_api.c  ;) --PMG */
        switch (buffer->buffer_type) {
        case MYSQL_TYPE_DOUBLE:
          if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
            PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\t\tst_fetch double data %f\n", fbh->ddata);
          sv_setnv(sv, fbh->ddata);
          break;

        case MYSQL_TYPE_LONG:
        case MYSQL_TYPE_LONGLONG:
          if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
            PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\t\tst_fetch int data %"IVdf", unsigned? %d\n",
                          fbh->ldata, buffer->is_unsigned);
          if (buffer->is_unsigned)
            sv_setuv(sv, fbh->ldata);
          else
            sv_setiv(sv, fbh->ldata);

          break;

        case MYSQL_TYPE_BIT:
          sv_setpvn(sv, fbh->data, fbh->length);

          break;

        default:
          if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
            PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\t\tERROR IN st_fetch_string");
          len= fbh->length;
	  /* ChopBlanks server-side prepared statement */
          if (ChopBlanks)
          {
            /* 
              see bottom of:
              http://www.mysql.org/doc/refman/5.0/en/c-api-datatypes.html
            */
            if (fbh->charsetnr != 63)
              while (len && fbh->data[len-1] == ' ') { --len; }
          }
	  /* END OF ChopBlanks */

          sv_setpvn(sv, fbh->data, len);

	/* UTF8 */
        /*HELMUT*/
#if defined(sv_utf8_decode) && MYSQL_VERSION_ID >=SERVER_PREPARE_VERSION

#if MYSQL_VERSION_ID >= FIELD_CHARSETNR_VERSION 
  /* SHOW COLLATION WHERE Id = 63; -- 63 == charset binary, collation binary */
        if ((imp_dbh->enable_utf8 || imp_dbh->enable_utf8mb4) && fbh->charsetnr != 63)
#else
	if ((imp_dbh->enable_utf8 || imp_dbh->enable_utf8mb4) && !(fbh->flags & BINARY_FLAG))
#endif
	  sv_utf8_decode(sv);
#endif
	/* END OF UTF8 */
          break;

        }

      }
    }

    if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
      PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\t<- dbd_st_fetch, %d cols\n", num_fields);

    return av;
  }
  else
  {
#endif

    imp_sth->currow++;

    if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
    {
      PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\tdbd_st_fetch result set details\n");
      PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\timp_sth->result=%p\n", imp_sth->result);
      PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\tmysql_num_fields=%u\n",
                    mysql_num_fields(imp_sth->result));
      PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\tmysql_num_rows=%llu\n",
                    mysql_num_rows(imp_sth->result));
      PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\tmysql_affected_rows=%llu\n",
                    mysql_affected_rows(imp_dbh->pmysql));
      PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\tdbd_st_fetch for %p, currow= %d\n",
                    sth,imp_sth->currow);
    }

    if (!(cols= mysql_fetch_row(imp_sth->result)))
    {
      if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
      {
        PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\tdbd_st_fetch, no more rows to fetch");
      }
      if (mysql_errno(imp_dbh->pmysql))
        do_error(sth, mysql_errno(imp_dbh->pmysql),
                 mysql_error(imp_dbh->pmysql),
                 mysql_sqlstate(imp_dbh->pmysql));


#if MYSQL_VERSION_ID >= MULTIPLE_RESULT_SET_VERSION
      if (!mysql_more_results(svsock))
#endif
        dbd_st_finish(sth, imp_sth);
      return Nullav;
    }

    num_fields= mysql_num_fields(imp_sth->result);
    fields= mysql_fetch_fields(imp_sth->result);
    lengths= mysql_fetch_lengths(imp_sth->result);

    if ((av= DBIc_FIELDS_AV(imp_sth)) != Nullav)
    {
      av_length= av_len(av)+1;

      if (av_length != num_fields)              /* Resize array if necessary */
      {
        if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
          PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\t<- dbd_st_fetch, size of results array(%d) != num_fields(%d)\n",
                                   av_length, num_fields);

        if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
          PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\t<- dbd_st_fetch, result fields(%d)\n",
                                   DBIc_NUM_FIELDS(imp_sth));

        av_readonly = SvREADONLY(av);

        if (av_readonly)
          SvREADONLY_off( av );              /* DBI sets this readonly */

        while (av_length < num_fields)
        {
          av_store(av, av_length++, newSV(0));
        }

        while (av_length > num_fields)
        {
          SvREFCNT_dec(av_pop(av));
          av_length--;
        }
        if (av_readonly)
          SvREADONLY_on(av);
      }
    }

    av= DBIc_DBISTATE(imp_sth)->get_fbav(imp_sth);

    for (i= 0;  i < num_fields; ++i)
    {
      char *col= cols[i];
      SV *sv= AvARRAY(av)[i]; /* Note: we (re)use the SV in the AV	*/

      if (col)
      {
        STRLEN len= lengths[i];
        if (ChopBlanks)
        {
          while (len && col[len-1] == ' ')
          {	--len; }
        }

        /* Set string value returned from mysql server */
        sv_setpvn(sv, col, len);

        switch (mysql_to_perl_type(fields[i].type)) {
        case MYSQL_TYPE_DOUBLE:
          /* Coerce to dobule and set scalar as NV */
          (void) SvNV(sv);
          SvNOK_only(sv);
          break;

        case MYSQL_TYPE_LONG:
        case MYSQL_TYPE_LONGLONG:
          /* Coerce to integer and set scalar as UV resp. IV */
          if (fields[i].flags & UNSIGNED_FLAG)
          {
            (void) SvUV(sv);
            SvIOK_only_UV(sv);
          }
          else
          {
            (void) SvIV(sv);
            SvIOK_only(sv);
          }
          break;

#if MYSQL_VERSION_ID > NEW_DATATYPE_VERSION
        case MYSQL_TYPE_BIT:
          /* Let it as binary string */
          break;
#endif

        default:
	/* UTF8 */
        /*HELMUT*/
#if defined(sv_utf8_decode) && MYSQL_VERSION_ID >=SERVER_PREPARE_VERSION

  /* see bottom of: http://www.mysql.org/doc/refman/5.0/en/c-api-datatypes.html */
        if ((imp_dbh->enable_utf8 || imp_dbh->enable_utf8mb4) && fields[i].charsetnr != 63)
	  sv_utf8_decode(sv);
#endif
	/* END OF UTF8 */
          break;
        }
      }
      else
        (void) SvOK_off(sv);  /*  Field is NULL, return undef  */
    }

    if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
      PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\t<- dbd_st_fetch, %d cols\n", num_fields);
    return av;

#if MYSQL_VERSION_ID  >= SERVER_PREPARE_VERSION
  }
#endif

}