bool st_select_lex::optimize_unflattened_subqueries(bool const_only)
{
  SELECT_LEX_UNIT *next_unit= NULL;
  for (SELECT_LEX_UNIT *un= first_inner_unit();
       un;
       un= next_unit ? next_unit : un->next_unit())
  {
    Item_subselect *subquery_predicate= un->item;
    next_unit= NULL;

    if (subquery_predicate)
    {
      if (!subquery_predicate->fixed)
      {
	/*
	 This subquery was excluded as part of some expression so it is
	 invisible from all prepared expression.
       */
	next_unit= un->next_unit();
	un->exclude_level();
	if (next_unit)
	  continue;
	break;
      }
      if (subquery_predicate->substype() == Item_subselect::IN_SUBS)
      {
        Item_in_subselect *in_subs= (Item_in_subselect*) subquery_predicate;
        if (in_subs->is_jtbm_merged)
          continue;
      }

      if (const_only && !subquery_predicate->const_item())
      {
        /* Skip non-constant subqueries if the caller asked so. */
        continue;
      }

      bool empty_union_result= true;
      bool is_correlated_unit= false;
      bool first= true;
      bool union_plan_saved= false;
      /*
        If the subquery is a UNION, optimize all the subqueries in the UNION. If
        there is no UNION, then the loop will execute once for the subquery.
      */
      for (SELECT_LEX *sl= un->first_select(); sl; sl= sl->next_select())
      {
        JOIN *inner_join= sl->join;
        if (first)
          first= false;
        else
        {
          if (!union_plan_saved)
          {
            union_plan_saved= true;
            if (un->save_union_explain(un->thd->lex->explain))
              return true; /* Failure */
          }
        }
        if (!inner_join)
          continue;
        SELECT_LEX *save_select= un->thd->lex->current_select;
        ulonglong save_options;
        int res;
        /* We need only 1 row to determine existence */
        un->set_limit(un->global_parameters());
        un->thd->lex->current_select= sl;
        save_options= inner_join->select_options;
        if (options & SELECT_DESCRIBE)
        {
          /* Optimize the subquery in the context of EXPLAIN. */
          sl->set_explain_type(FALSE);
          sl->options|= SELECT_DESCRIBE;
          inner_join->select_options|= SELECT_DESCRIBE;
        }
        if ((res= inner_join->optimize()))
          return TRUE;
        if (!inner_join->cleaned)
          sl->update_used_tables();
        sl->update_correlated_cache();
        is_correlated_unit|= sl->is_correlated;
        inner_join->select_options= save_options;
        un->thd->lex->current_select= save_select;

        Explain_query *eq;
        if ((eq= inner_join->thd->lex->explain))
        {
          Explain_select *expl_sel;
          if ((expl_sel= eq->get_select(inner_join->select_lex->select_number)))
          {
            sl->set_explain_type(TRUE);
            expl_sel->select_type= sl->type;
          }
        }

        if (empty_union_result)
        {
          /*
            If at least one subquery in a union is non-empty, the UNION result
            is non-empty. If there is no UNION, the only subquery is non-empy.
          */
          empty_union_result= inner_join->empty_result();
        }
        if (res)
          return TRUE;
      }
      if (empty_union_result)
        subquery_predicate->no_rows_in_result();
      if (!is_correlated_unit)
        un->uncacheable&= ~UNCACHEABLE_DEPENDENT;
      subquery_predicate->is_correlated= is_correlated_unit;
    }
  }
  return FALSE;
}