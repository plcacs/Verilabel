bool open_table(THD *thd, TABLE_LIST *table_list, Open_table_context *ot_ctx)
{
  TABLE *table;
  const char *key;
  uint	key_length;
  const char *alias= table_list->alias.str;
  uint flags= ot_ctx->get_flags();
  MDL_ticket *mdl_ticket;
  TABLE_SHARE *share;
  uint gts_flags;
  bool from_share= false;
#ifdef WITH_PARTITION_STORAGE_ENGINE
  int part_names_error=0;
#endif
  DBUG_ENTER("open_table");

  /*
    The table must not be opened already. The table can be pre-opened for
    some statements if it is a temporary table.

    open_temporary_table() must be used to open temporary tables.
  */
  DBUG_ASSERT(!table_list->table);

  /* an open table operation needs a lot of the stack space */
  if (check_stack_overrun(thd, STACK_MIN_SIZE_FOR_OPEN, (uchar *)&alias))
    DBUG_RETURN(TRUE);

  if (!(flags & MYSQL_OPEN_IGNORE_KILLED) && thd->killed)
  {
    thd->send_kill_message();
    DBUG_RETURN(TRUE);
  }

  /*
    Check if we're trying to take a write lock in a read only transaction.

    Note that we allow write locks on log tables as otherwise logging
    to general/slow log would be disabled in read only transactions.
  */
  if (table_list->mdl_request.is_write_lock_request() &&
      thd->tx_read_only &&
      !(flags & (MYSQL_LOCK_LOG_TABLE | MYSQL_OPEN_HAS_MDL_LOCK)))
  {
    my_error(ER_CANT_EXECUTE_IN_READ_ONLY_TRANSACTION, MYF(0));
    DBUG_RETURN(true);
  }

  if (!table_list->db.str)
  {
    my_error(ER_NO_DB_ERROR, MYF(0));
    DBUG_RETURN(true);
  }

  key_length= get_table_def_key(table_list, &key);

  /*
    If we're in pre-locked or LOCK TABLES mode, let's try to find the
    requested table in the list of pre-opened and locked tables. If the
    table is not there, return an error - we can't open not pre-opened
    tables in pre-locked/LOCK TABLES mode.
    TODO: move this block into a separate function.
  */
  if (thd->locked_tables_mode &&
      ! (flags & MYSQL_OPEN_GET_NEW_TABLE))
  {						// Using table locks
    TABLE *best_table= 0;
    int best_distance= INT_MIN;
    for (table=thd->open_tables; table ; table=table->next)
    {
      if (table->s->table_cache_key.length == key_length &&
	  !memcmp(table->s->table_cache_key.str, key, key_length))
      {
        if (!my_strcasecmp(system_charset_info, table->alias.c_ptr(), alias) &&
            table->query_id != thd->query_id && /* skip tables already used */
            (thd->locked_tables_mode == LTM_LOCK_TABLES ||
             table->query_id == 0))
        {
          int distance= ((int) table->reginfo.lock_type -
                         (int) table_list->lock_type);

          /*
            Find a table that either has the exact lock type requested,
            or has the best suitable lock. In case there is no locked
            table that has an equal or higher lock than requested,
            we us the closest matching lock to be able to produce an error
            message about wrong lock mode on the table. The best_table
            is changed if bd < 0 <= d or bd < d < 0 or 0 <= d < bd.

            distance <  0 - No suitable lock found
            distance >  0 - we have lock mode higher then we require
            distance == 0 - we have lock mode exactly which we need
          */
          if ((best_distance < 0 && distance > best_distance) ||
              (distance >= 0 && distance < best_distance))
          {
            best_distance= distance;
            best_table= table;
            if (best_distance == 0)
            {
              /*
                We have found a perfect match and can finish iterating
                through open tables list. Check for table use conflict
                between calling statement and SP/trigger is done in
                lock_tables().
              */
              break;
            }
          }
        }
      }
    }
    if (best_table)
    {
      table= best_table;
      table->query_id= thd->query_id;
      table->init(thd, table_list);
      DBUG_PRINT("info",("Using locked table"));
#ifdef WITH_PARTITION_STORAGE_ENGINE
      part_names_error= set_partitions_as_used(table_list, table);
#endif
      goto reset;
    }

    if (is_locked_view(thd, table_list))
    {
      if (table_list->sequence)
      {
        my_error(ER_NOT_SEQUENCE, MYF(0), table_list->db.str, table_list->alias.str);
        DBUG_RETURN(true);
      }
      DBUG_RETURN(FALSE); // VIEW
    }

    /*
      No table in the locked tables list. In case of explicit LOCK TABLES
      this can happen if a user did not include the table into the list.
      In case of pre-locked mode locked tables list is generated automatically,
      so we may only end up here if the table did not exist when
      locked tables list was created.
    */
    if (thd->locked_tables_mode == LTM_PRELOCKED)
      my_error(ER_NO_SUCH_TABLE, MYF(0), table_list->db.str, table_list->alias.str);
    else
      my_error(ER_TABLE_NOT_LOCKED, MYF(0), alias);
    DBUG_RETURN(TRUE);
  }

  /*
    Non pre-locked/LOCK TABLES mode, and the table is not temporary.
    This is the normal use case.
  */

  if (! (flags & MYSQL_OPEN_HAS_MDL_LOCK))
  {
    /*
      We are not under LOCK TABLES and going to acquire write-lock/
      modify the base table. We need to acquire protection against
      global read lock until end of this statement in order to have
      this statement blocked by active FLUSH TABLES WITH READ LOCK.

      We don't need to acquire this protection under LOCK TABLES as
      such protection already acquired at LOCK TABLES time and
      not released until UNLOCK TABLES.

      We don't block statements which modify only temporary tables
      as these tables are not preserved by any form of
      backup which uses FLUSH TABLES WITH READ LOCK.

      TODO: The fact that we sometimes acquire protection against
            GRL only when we encounter table to be write-locked
            slightly increases probability of deadlock.
            This problem will be solved once Alik pushes his
            temporary table refactoring patch and we can start
            pre-acquiring metadata locks at the beggining of
            open_tables() call.
    */
    if (table_list->mdl_request.is_write_lock_request() &&
        ! (flags & (MYSQL_OPEN_IGNORE_GLOBAL_READ_LOCK |
                    MYSQL_OPEN_FORCE_SHARED_MDL |
                    MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL |
                    MYSQL_OPEN_SKIP_SCOPED_MDL_LOCK)) &&
        ! ot_ctx->has_protection_against_grl())
    {
      MDL_request protection_request;
      MDL_deadlock_handler mdl_deadlock_handler(ot_ctx);

      if (thd->global_read_lock.can_acquire_protection())
        DBUG_RETURN(TRUE);

      protection_request.init(MDL_key::GLOBAL, "", "", MDL_INTENTION_EXCLUSIVE,
                              MDL_STATEMENT);

      /*
        Install error handler which if possible will convert deadlock error
        into request to back-off and restart process of opening tables.
      */
      thd->push_internal_handler(&mdl_deadlock_handler);
      bool result= thd->mdl_context.acquire_lock(&protection_request,
                                                 ot_ctx->get_timeout());
      thd->pop_internal_handler();

      if (result)
        DBUG_RETURN(TRUE);

      ot_ctx->set_has_protection_against_grl();
    }

    if (open_table_get_mdl_lock(thd, ot_ctx, &table_list->mdl_request,
                                flags, &mdl_ticket) ||
        mdl_ticket == NULL)
    {
      DEBUG_SYNC(thd, "before_open_table_wait_refresh");
      DBUG_RETURN(TRUE);
    }
    DEBUG_SYNC(thd, "after_open_table_mdl_shared");
  }
  else
  {
    /*
      Grab reference to the MDL lock ticket that was acquired
      by the caller.
    */
    mdl_ticket= table_list->mdl_request.ticket;
  }

  if (table_list->open_strategy == TABLE_LIST::OPEN_IF_EXISTS)
  {
    if (!ha_table_exists(thd, &table_list->db, &table_list->table_name))
      DBUG_RETURN(FALSE);
  }
  else if (table_list->open_strategy == TABLE_LIST::OPEN_STUB)
    DBUG_RETURN(FALSE);

  /* Table exists. Let us try to open it. */

  if (table_list->i_s_requested_object & OPEN_TABLE_ONLY)
    gts_flags= GTS_TABLE;
  else if (table_list->i_s_requested_object &  OPEN_VIEW_ONLY)
    gts_flags= GTS_VIEW;
  else
    gts_flags= GTS_TABLE | GTS_VIEW;

retry_share:

  share= tdc_acquire_share(thd, table_list, gts_flags, &table);

  if (unlikely(!share))
  {
    /*
      Hide "Table doesn't exist" errors if the table belongs to a view.
      The check for thd->is_error() is necessary to not push an
      unwanted error in case the error was already silenced.
      @todo Rework the alternative ways to deal with ER_NO_SUCH TABLE.
    */
    if (thd->is_error())
    {
      if (table_list->parent_l)
      {
        thd->clear_error();
        my_error(ER_WRONG_MRG_TABLE, MYF(0));
      }
      else if (table_list->belong_to_view)
      {
        TABLE_LIST *view= table_list->belong_to_view;
        thd->clear_error();
        my_error(ER_VIEW_INVALID, MYF(0),
                 view->view_db.str, view->view_name.str);
      }
    }
    DBUG_RETURN(TRUE);
  }

  /*
    Check if this TABLE_SHARE-object corresponds to a view. Note, that there is
    no need to check TABLE_SHARE::tdc.flushed as we do for regular tables,
    because view shares are always up to date.
  */
  if (share->is_view)
  {
    /*
      If parent_l of the table_list is non null then a merge table
      has this view as child table, which is not supported.
    */
    if (table_list->parent_l)
    {
      my_error(ER_WRONG_MRG_TABLE, MYF(0));
      goto err_lock;
    }
    if (table_list->sequence)
    {
      my_error(ER_NOT_SEQUENCE, MYF(0), table_list->db.str,
               table_list->alias.str);
      goto err_lock;
    }
    /*
      This table is a view. Validate its metadata version: in particular,
      that it was a view when the statement was prepared.
    */
    if (check_and_update_table_version(thd, table_list, share))
      goto err_lock;

    /* Open view */
    if (mysql_make_view(thd, share, table_list, false))
      goto err_lock;


    /* TODO: Don't free this */
    tdc_release_share(share);

    DBUG_ASSERT(table_list->view);

    DBUG_RETURN(FALSE);
  }

#ifdef WITH_WSREP
  if (!((flags & MYSQL_OPEN_IGNORE_FLUSH) ||
        (thd->wsrep_applier)))
#else
  if (!(flags & MYSQL_OPEN_IGNORE_FLUSH))
#endif
  {
    if (share->tdc->flushed)
    {
      DBUG_PRINT("info", ("Found old share version: %lld  current: %lld",
                          share->tdc->version, tdc_refresh_version()));
      /*
        We already have an MDL lock. But we have encountered an old
        version of table in the table definition cache which is possible
        when someone changes the table version directly in the cache
        without acquiring a metadata lock (e.g. this can happen during
        "rolling" FLUSH TABLE(S)).
        Release our reference to share, wait until old version of
        share goes away and then try to get new version of table share.
      */
      if (table)
        tc_release_table(table);
      else
        tdc_release_share(share);

      MDL_deadlock_handler mdl_deadlock_handler(ot_ctx);
      bool wait_result;

      thd->push_internal_handler(&mdl_deadlock_handler);
      wait_result= tdc_wait_for_old_version(thd, table_list->db.str,
                                            table_list->table_name.str,
                                            ot_ctx->get_timeout(),
                                            mdl_ticket->get_deadlock_weight());
      thd->pop_internal_handler();

      if (wait_result)
        DBUG_RETURN(TRUE);

      goto retry_share;
    }

    if (thd->open_tables && thd->open_tables->s->tdc->flushed)
    {
      /*
        If the version changes while we're opening the tables,
        we have to back off, close all the tables opened-so-far,
        and try to reopen them. Note: refresh_version is currently
        changed only during FLUSH TABLES.
      */
      if (table)
        tc_release_table(table);
      else
        tdc_release_share(share);
      (void)ot_ctx->request_backoff_action(Open_table_context::OT_REOPEN_TABLES,
                                           NULL);
      DBUG_RETURN(TRUE);
    }
  }

  if (table)
  {
    DBUG_ASSERT(table->file != NULL);
    MYSQL_REBIND_TABLE(table->file);
#ifdef WITH_PARTITION_STORAGE_ENGINE
    part_names_error= set_partitions_as_used(table_list, table);
#endif
  }
  else
  {
    enum open_frm_error error;

    /* make a new table */
    if (!(table=(TABLE*) my_malloc(sizeof(*table),MYF(MY_WME))))
      goto err_lock;

    error= open_table_from_share(thd, share, &table_list->alias,
                                 HA_OPEN_KEYFILE | HA_TRY_READ_ONLY,
                                 EXTRA_RECORD,
                                 thd->open_options, table, FALSE,
                                 IF_PARTITIONING(table_list->partition_names,0));

    if (unlikely(error))
    {
      my_free(table);

      if (error == OPEN_FRM_DISCOVER)
        (void) ot_ctx->request_backoff_action(Open_table_context::OT_DISCOVER,
                                              table_list);
      else if (share->crashed)
      {
        if (!(flags & MYSQL_OPEN_IGNORE_REPAIR))
          (void) ot_ctx->request_backoff_action(Open_table_context::OT_REPAIR,
                                                table_list);
        else
          table_list->crashed= 1;  /* Mark that table was crashed */
      }
      goto err_lock;
    }
    if (open_table_entry_fini(thd, share, table))
    {
      closefrm(table);
      my_free(table);
      goto err_lock;
    }

    /* Add table to the share's used tables list. */
    tc_add_table(thd, table);
    from_share= true;
  }

  table->mdl_ticket= mdl_ticket;
  table->reginfo.lock_type=TL_READ;		/* Assume read */

  table->init(thd, table_list);

  table->next= thd->open_tables;		/* Link into simple list */
  thd->set_open_tables(table);

 reset:
  /*
    Check that there is no reference to a condition from an earlier query
    (cf. Bug#58553). 
  */
  DBUG_ASSERT(table->file->pushed_cond == NULL);
  table_list->updatable= 1; // It is not derived table nor non-updatable VIEW
  table_list->table= table;

  if (!from_share && table->vcol_fix_expr(thd))
    goto err_lock;

#ifdef WITH_PARTITION_STORAGE_ENGINE
  if (unlikely(table->part_info))
  {
    /* Partitions specified were incorrect.*/
    if (part_names_error)
    {
      table->file->print_error(part_names_error, MYF(0));
      DBUG_RETURN(true);
    }
  }
  else if (table_list->partition_names)
  {
    /* Don't allow PARTITION () clause on a nonpartitioned table */
    my_error(ER_PARTITION_CLAUSE_ON_NONPARTITIONED, MYF(0));
    DBUG_RETURN(true);
  }
#endif
  if (table_list->sequence && table->s->table_type != TABLE_TYPE_SEQUENCE)
  {
    my_error(ER_NOT_SEQUENCE, MYF(0), table_list->db.str, table_list->alias.str);
    DBUG_RETURN(true);
  }

  DBUG_RETURN(FALSE);

err_lock:
  tdc_release_share(share);

  DBUG_PRINT("exit", ("failed"));
  DBUG_RETURN(TRUE);
}