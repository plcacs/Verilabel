LJ_NOINLINE void lj_err_run(lua_State *L)
{
  ptrdiff_t ef = finderrfunc(L);
  if (ef) {
    TValue *errfunc = restorestack(L, ef);
    TValue *top = L->top;
    lj_trace_abort(G(L));
    if (!tvisfunc(errfunc) || L->status == LUA_ERRERR) {
      setstrV(L, top-1, lj_err_str(L, LJ_ERR_ERRERR));
      lj_err_throw(L, LUA_ERRERR);
    }
    L->status = LUA_ERRERR;
    copyTV(L, top, top-1);
    copyTV(L, top-1, errfunc);
    L->top = top+1;
    lj_vm_call(L, top, 1+1);  /* Stack: |errfunc|msg| -> |msg| */
  }
  lj_err_throw(L, LUA_ERRRUN);
}