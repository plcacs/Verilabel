Event_job_data::execute(THD *thd, bool drop)
{
  String sp_sql;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  Security_context event_sctx, *save_sctx= NULL;
#endif
  List<Item> empty_item_list;
  bool ret= TRUE;

  DBUG_ENTER("Event_job_data::execute");

  thd->reset_for_next_command();

  /*
    MySQL parser currently assumes that current database is either
    present in THD or all names in all statements are fully specified.
    And yet not fully specified names inside stored programs must be 
    be supported, even if the current database is not set:
    CREATE PROCEDURE db1.p1() BEGIN CREATE TABLE t1; END//
    -- in this example t1 should be always created in db1 and the statement
    must parse even if there is no current database.

    To support this feature and still address the parser limitation,
    we need to set the current database here.
    We don't have to call mysql_change_db, since the checks performed
    in it are unnecessary for the purpose of parsing, and
    mysql_change_db will be invoked anyway later, to activate the
    procedure database before it's executed.
  */
  thd->set_db(dbname.str, dbname.length);

  lex_start(thd);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (event_sctx.change_security_context(thd,
                                         &definer_user, &definer_host,
                                         &dbname, &save_sctx))
  {
    sql_print_error("Event Scheduler: "
                    "[%s].[%s.%s] execution failed, "
                    "failed to authenticate the user.",
                    definer.str, dbname.str, name.str);
    goto end;
  }
#endif

  if (check_access(thd, EVENT_ACL, dbname.str, NULL, NULL, 0, 0))
  {
    /*
      This aspect of behavior is defined in the worklog,
      and this is how triggers work too: if TRIGGER
      privilege is revoked from trigger definer,
      triggers are not executed.
    */
    sql_print_error("Event Scheduler: "
                    "[%s].[%s.%s] execution failed, "
                    "user no longer has EVENT privilege.",
                    definer.str, dbname.str, name.str);
    goto end;
  }

  if (construct_sp_sql(thd, &sp_sql))
    goto end;

  /*
    Set up global thread attributes to reflect the properties of
    this Event. We can simply reset these instead of usual
    backup/restore employed in stored programs since we know that
    this is a top level statement and the worker thread is
    allocated exclusively to execute this event.
  */

  thd->variables.sql_mode= sql_mode;
  thd->variables.time_zone= time_zone;

  thd->set_query(sp_sql.c_ptr_safe(), sp_sql.length());

  {
    Parser_state parser_state;
    if (parser_state.init(thd, thd->query(), thd->query_length()))
      goto end;

    if (parse_sql(thd, & parser_state, creation_ctx))
    {
      sql_print_error("Event Scheduler: "
                      "%serror during compilation of %s.%s",
                      thd->is_fatal_error ? "fatal " : "",
                      (const char *) dbname.str, (const char *) name.str);
      goto end;
    }
  }

  {
    sp_head *sphead= thd->lex->sphead;

    DBUG_ASSERT(sphead);

    sphead->m_flags|= sp_head::LOG_SLOW_STATEMENTS;
    sphead->m_flags|= sp_head::LOG_GENERAL_LOG;

    sphead->set_info(0, 0, &thd->lex->sp_chistics, sql_mode);
    sphead->set_creation_ctx(creation_ctx);
    sphead->optimize();

    ret= sphead->execute_procedure(thd, &empty_item_list);
    /*
      There is no pre-locking and therefore there should be no
      tables open and locked left after execute_procedure.
    */
  }

end:
  if (drop && !thd->is_fatal_error)
  {
    /*
      We must do it here since here we're under the right authentication
      ID of the event definer.
    */
    sql_print_information("Event Scheduler: Dropping %s.%s",
                          (const char *) dbname.str, (const char *) name.str);
    /*
      Construct a query for the binary log, to ensure the event is dropped
      on the slave
    */
    if (construct_drop_event_sql(thd, &sp_sql))
      ret= 1;
    else
    {
      ulong saved_master_access;

      thd->set_query(sp_sql.c_ptr_safe(), sp_sql.length());

      /*
        NOTE: even if we run in read-only mode, we should be able to lock
        the mysql.event table for writing. In order to achieve this, we
        should call mysql_lock_tables() under the super-user.

        Same goes for transaction access mode.
        Temporarily reset it to read-write.
      */

      saved_master_access= thd->security_ctx->master_access;
      thd->security_ctx->master_access |= SUPER_ACL;
      bool save_tx_read_only= thd->tx_read_only;
      thd->tx_read_only= false;

      if (WSREP(thd))
      {
        thd->lex->sql_command = SQLCOM_DROP_EVENT;
        WSREP_TO_ISOLATION_BEGIN(WSREP_MYSQL_DB, NULL, NULL);
      }

      ret= Events::drop_event(thd, dbname, name, FALSE);

      WSREP_TO_ISOLATION_END;

#ifdef WITH_WSREP
  error:
#endif
      thd->tx_read_only= save_tx_read_only;
      thd->security_ctx->master_access= saved_master_access;
    }
  }
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (save_sctx)
    event_sctx.restore_security_context(thd, save_sctx);
#endif
  thd->lex->unit.cleanup();
  thd->end_statement();
  thd->cleanup_after_query();
  /* Avoid races with SHOW PROCESSLIST */
  thd->reset_query();

  DBUG_PRINT("info", ("EXECUTED %s.%s  ret: %d", dbname.str, name.str, ret));

  DBUG_RETURN(ret);
}