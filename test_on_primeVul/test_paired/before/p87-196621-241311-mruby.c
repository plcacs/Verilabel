mrb_remove_method(mrb_state *mrb, struct RClass *c, mrb_sym mid)
{
  mt_tbl *h;

  MRB_CLASS_ORIGIN(c);
  h = c->mt;

  if (h && mt_del(mrb, h, mid)) return;
  mrb_name_error(mrb, mid, "method '%n' not defined in %C", mid, c);
}