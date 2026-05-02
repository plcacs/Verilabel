bool Item_singlerow_subselect::fix_length_and_dec()
{
  if ((max_columns= engine->cols()) == 1)
  {
    if (engine->fix_length_and_dec(row= &value))
      return TRUE;
  }
  else
  {
    if (!(row= (Item_cache**) current_thd->alloc(sizeof(Item_cache*) *
                                                 max_columns)) ||
        engine->fix_length_and_dec(row))
      return TRUE;
    value= *row;
  }
  unsigned_flag= value->unsigned_flag;
  /*
    If the subquery has no tables (1) and is not a UNION (2), like:

      (SELECT subq_value)

    then its NULLability is the same as subq_value's NULLability.

    (1): A subquery that uses a table will return NULL when the table is empty.
    (2): A UNION subquery will return NULL if it produces a "Subquery returns
         more than one row" error.
  */
  if (engine->no_tables() &&
      engine->engine_type() != subselect_engine::UNION_ENGINE)
    maybe_null= engine->may_be_null();
  else
  {
    for (uint i= 0; i < max_columns; i++)
      row[i]->maybe_null= TRUE;
  }
  return FALSE;
}