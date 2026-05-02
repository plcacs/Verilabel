mrb_realloc(mrb_state *mrb, void *p, size_t len)
{
  void *p2;

  p2 = mrb_realloc_simple(mrb, p, len);
  if (len == 0) return p2;
  if (p2 == NULL) {
    mrb_free(mrb, p);
    mrb->gc.out_of_memory = TRUE;
    mrb_raise_nomemory(mrb);
  }
  else {
    mrb->gc.out_of_memory = FALSE;
  }

  return p2;
}