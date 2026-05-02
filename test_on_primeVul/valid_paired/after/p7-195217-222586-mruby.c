mrb_proc_copy(mrb_state *mrb, struct RProc *a, struct RProc *b)
{
  if (a->body.irep) {
    /* already initialized proc */
    return;
  }
  if (!MRB_PROC_CFUNC_P(b) && b->body.irep) {
    mrb_irep_incref(mrb, (mrb_irep*)b->body.irep);
  }
  a->flags = b->flags;
  a->body = b->body;
  a->upper = b->upper;
  a->e.env = b->e.env;
  /* a->e.target_class = a->e.target_class; */
}