void ha_maria::drop_table(const char *name)
{
  DBUG_ASSERT(file->s->temporary);
  (void) ha_close();
  (void) maria_delete_table_files(name, 1, MY_WME);
}