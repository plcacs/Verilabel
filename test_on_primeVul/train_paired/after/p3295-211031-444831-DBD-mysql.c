int dbd_bind_ph(SV *sth, imp_sth_t *imp_sth, SV *param, SV *value,
		 IV sql_type, SV *attribs, int is_inout, IV maxlen) {
  dTHX;
  int rc;
  int param_num= SvIV(param);
  int idx= param_num - 1;
  char *err_msg;
  D_imp_xxh(sth);

#if MYSQL_VERSION_ID >= SERVER_PREPARE_VERSION
  STRLEN slen;
  char *buffer= NULL;
  int buffer_is_null= 0;
  int buffer_length= slen;
  unsigned int buffer_type= 0;
  IV tmp;
#endif

  D_imp_dbh_from_sth;
  ASYNC_CHECK_RETURN(sth, FALSE);

  if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
    PerlIO_printf(DBIc_LOGPIO(imp_xxh),
                  "   Called: dbd_bind_ph\n");

  attribs= attribs;
  maxlen= maxlen;

  if (param_num <= 0  ||  param_num > DBIc_NUM_PARAMS(imp_sth))
  {
    do_error(sth, JW_ERR_ILLEGAL_PARAM_NUM, "Illegal parameter number", NULL);
    return FALSE;
  }

  /*
     This fixes the bug whereby no warning was issued upon binding a
     defined non-numeric as numeric
   */
  if (SvOK(value) &&
      (sql_type == SQL_NUMERIC  ||
       sql_type == SQL_DECIMAL  ||
       sql_type == SQL_INTEGER  ||
       sql_type == SQL_SMALLINT ||
       sql_type == SQL_FLOAT    ||
       sql_type == SQL_REAL     ||
       sql_type == SQL_DOUBLE) )
  {
    if (! looks_like_number(value))
    {
      err_msg = SvPVX(sv_2mortal(newSVpvf(
              "Binding non-numeric field %d, value %s as a numeric!",
              param_num, neatsvpv(value,0))));
      do_error(sth, JW_ERR_ILLEGAL_PARAM_NUM, err_msg, NULL);
    }
  }

  if (is_inout)
  {
    do_error(sth, JW_ERR_NOT_IMPLEMENTED, "Output parameters not implemented", NULL);
    return FALSE;
  }

  rc = bind_param(&imp_sth->params[idx], value, sql_type);

#if MYSQL_VERSION_ID >= SERVER_PREPARE_VERSION
  if (imp_sth->use_server_side_prepare)
  {
      switch(sql_type) {
      case SQL_NUMERIC:
      case SQL_INTEGER:
      case SQL_SMALLINT:
      case SQL_BIGINT:
      case SQL_TINYINT:
          buffer_type= MYSQL_TYPE_LONG;
          break;
      case SQL_DOUBLE:
      case SQL_DECIMAL: 
      case SQL_FLOAT: 
      case SQL_REAL:
          buffer_type= MYSQL_TYPE_DOUBLE;
          break;
      case SQL_CHAR: 
      case SQL_VARCHAR: 
      case SQL_DATE: 
      case SQL_TIME: 
      case SQL_TIMESTAMP: 
      case SQL_LONGVARCHAR: 
      case SQL_BINARY: 
      case SQL_VARBINARY: 
      case SQL_LONGVARBINARY:
          buffer_type= MYSQL_TYPE_BLOB;
          break;
      default:
          buffer_type= MYSQL_TYPE_STRING;
    }
    buffer_is_null = !(SvOK(imp_sth->params[idx].value) && imp_sth->params[idx].value);
    if (! buffer_is_null) {
      switch(buffer_type) {
        case MYSQL_TYPE_LONG:
          /* INT */
          if (!SvIOK(imp_sth->params[idx].value) && DBIc_TRACE_LEVEL(imp_xxh) >= 2)
            PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\t\tTRY TO BIND AN INT NUMBER\n");
          buffer_length = sizeof imp_sth->fbind[idx].numeric_val.lval;

          tmp = SvIV(imp_sth->params[idx].value);
          if (tmp > INT32_MAX)
	        croak("Could not bind %ld: Integer too large for MYSQL_TYPE_LONG", tmp);
          imp_sth->fbind[idx].numeric_val.lval= tmp;
          buffer=(void*)&(imp_sth->fbind[idx].numeric_val.lval);
          if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
            PerlIO_printf(DBIc_LOGPIO(imp_xxh),
                          "   SCALAR type %d ->%"PRId32"<- IS A INT NUMBER\n",
                          (int) sql_type, *(int32_t *)buffer);
          break;

        case MYSQL_TYPE_DOUBLE:
          if (!SvNOK(imp_sth->params[idx].value) && DBIc_TRACE_LEVEL(imp_xxh) >= 2)
            PerlIO_printf(DBIc_LOGPIO(imp_xxh), "\t\tTRY TO BIND A FLOAT NUMBER\n");
          buffer_length = sizeof imp_sth->fbind[idx].numeric_val.dval;
          imp_sth->fbind[idx].numeric_val.dval= SvNV(imp_sth->params[idx].value);
          buffer=(char*)&(imp_sth->fbind[idx].numeric_val.dval);
          if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
            PerlIO_printf(DBIc_LOGPIO(imp_xxh),
                          "   SCALAR type %d ->%f<- IS A FLOAT NUMBER\n",
                          (int) sql_type, (double)(*buffer));
          break;

        case MYSQL_TYPE_BLOB:
          if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
            PerlIO_printf(DBIc_LOGPIO(imp_xxh),
                          "   SCALAR type BLOB\n");
          break;

        case MYSQL_TYPE_STRING:
          if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
            PerlIO_printf(DBIc_LOGPIO(imp_xxh),
                          "   SCALAR type STRING %d, buffertype=%d\n", (int) sql_type, buffer_type);
          break;

        default:
          croak("Bug in DBD::Mysql file dbdimp.c#dbd_bind_ph: do not know how to handle unknown buffer type.");
      }

      if (buffer_type == MYSQL_TYPE_STRING || buffer_type == MYSQL_TYPE_BLOB)
      {
        buffer= SvPV(imp_sth->params[idx].value, slen);
        buffer_length= slen;
        if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
          PerlIO_printf(DBIc_LOGPIO(imp_xxh),
                        " SCALAR type %d ->length %d<- IS A STRING or BLOB\n",
                        (int) sql_type, buffer_length);
      }
    }
    else
    {
      /*case: buffer_is_null != 0*/
      buffer= NULL;
      if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
        PerlIO_printf(DBIc_LOGPIO(imp_xxh),
                      "   SCALAR NULL VALUE: buffer type is: %d\n", buffer_type);
    }

    /* Type of column was changed. Force to rebind */
    if (imp_sth->bind[idx].buffer_type != buffer_type) {
      /* Note: this looks like being another bug:
       * if type of parameter N changes, then a bind is triggered
       * with an only partially filled bind structure ??
       */
      if (DBIc_TRACE_LEVEL(imp_xxh) >= 2)
          PerlIO_printf(DBIc_LOGPIO(imp_xxh),
                        "   FORCE REBIND: buffer type changed from %d to %d, sql-type=%d\n",
                        (int) imp_sth->bind[idx].buffer_type, buffer_type, (int) sql_type);
      imp_sth->has_been_bound = 0;
    }

    /* prepare has not been called */
    if (imp_sth->has_been_bound == 0)
    {
      imp_sth->bind[idx].buffer_type= buffer_type;
      imp_sth->bind[idx].buffer= buffer;
      imp_sth->bind[idx].buffer_length= buffer_length;
    }
    else /* prepare has been called */
    {
      imp_sth->stmt->params[idx].buffer= buffer;
      imp_sth->stmt->params[idx].buffer_length= buffer_length;
    }
    imp_sth->fbind[idx].length= buffer_length;
    imp_sth->fbind[idx].is_null= buffer_is_null;
  }
#endif
  return rc;
}