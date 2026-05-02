int setup_conds(THD *thd, TABLE_LIST *tables, List<TABLE_LIST> &leaves,
                COND **conds)
{
  SELECT_LEX *select_lex= thd->lex->current_select;
  TABLE_LIST *table= NULL;	// For HP compilers
  /*
    it_is_update set to TRUE when tables of primary SELECT_LEX (SELECT_LEX
    which belong to LEX, i.e. most up SELECT) will be updated by
    INSERT/UPDATE/LOAD
    NOTE: using this condition helps to prevent call of prepare_check_option()
    from subquery of VIEW, because tables of subquery belongs to VIEW
    (see condition before prepare_check_option() call)
  */
  bool it_is_update= (select_lex == thd->lex->first_select_lex()) &&
    thd->lex->which_check_option_applicable();
  bool save_is_item_list_lookup= select_lex->is_item_list_lookup;
  TABLE_LIST *derived= select_lex->master_unit()->derived;
  DBUG_ENTER("setup_conds");

  select_lex->is_item_list_lookup= 0;

  thd->column_usage= MARK_COLUMNS_READ;
  DBUG_PRINT("info", ("thd->column_usage: %d", thd->column_usage));
  select_lex->cond_count= 0;
  select_lex->between_count= 0;
  select_lex->max_equal_elems= 0;

  for (table= tables; table; table= table->next_local)
  {
    if (select_lex == thd->lex->first_select_lex() &&
        select_lex->first_cond_optimization &&
        table->merged_for_insert &&
        table->prepare_where(thd, conds, FALSE))
      goto err_no_arena;
  }

  if (*conds)
  {
    thd->where="where clause";
    DBUG_EXECUTE("where",
                 print_where(*conds,
                             "WHERE in setup_conds",
                             QT_ORDINARY););
    /*
      Wrap alone field in WHERE clause in case it will be outer field of subquery
      which need persistent pointer on it, but conds could be changed by optimizer
    */
    if ((*conds)->type() == Item::FIELD_ITEM && !derived)
      wrap_ident(thd, conds);
    (*conds)->mark_as_condition_AND_part(NO_JOIN_NEST);
    if ((*conds)->fix_fields_if_needed_for_bool(thd, conds))
      goto err_no_arena;
  }

  /*
    Apply fix_fields() to all ON clauses at all levels of nesting,
    including the ones inside view definitions.
  */
  if (setup_on_expr(thd, tables, it_is_update))
    goto err_no_arena;

  if (!thd->stmt_arena->is_conventional())
  {
    /*
      We are in prepared statement preparation code => we should store
      WHERE clause changing for next executions.

      We do this ON -> WHERE transformation only once per PS/SP statement.
    */
    select_lex->where= *conds;
  }
  thd->lex->current_select->is_item_list_lookup= save_is_item_list_lookup;
  DBUG_RETURN(thd->is_error());

err_no_arena:
  select_lex->is_item_list_lookup= save_is_item_list_lookup;
  DBUG_RETURN(1);
}