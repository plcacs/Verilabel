static void fix_dl_name(MEM_ROOT *root, LEX_STRING *dl)
{
  const size_t so_ext_len= sizeof(SO_EXT) - 1;
  if (my_strcasecmp(&my_charset_latin1, dl->str + dl->length - so_ext_len,
                    SO_EXT))
  {
    char *s= (char*)alloc_root(root, dl->length + so_ext_len + 1);
    memcpy(s, dl->str, dl->length);
    strcpy(s + dl->length, SO_EXT);
    dl->str= s;
    dl->length+= so_ext_len;
  }
}