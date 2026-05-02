static void mysql_prune_stmt_list(MYSQL *mysql)
{
  LIST *element= mysql->stmts;
  LIST *pruned_list= 0;

  for (; element; element= element->next)
  {
    MYSQL_STMT *stmt= (MYSQL_STMT *) element->data;
    if (stmt->state != MYSQL_STMT_INIT_DONE)
    {
      stmt->mysql= 0;
      stmt->last_errno= CR_SERVER_LOST;
      strmov(stmt->last_error, ER(CR_SERVER_LOST));
      strmov(stmt->sqlstate, unknown_sqlstate);
    }
    else
    {
      pruned_list= list_add(pruned_list, element);
    }
  }

  mysql->stmts= pruned_list;
}