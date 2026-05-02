l_noret luaG_runerror (lua_State *L, const char *fmt, ...) {
  CallInfo *ci = L->ci;
  const char *msg;
  va_list argp;
  luaC_checkGC(L);  /* error message uses memory */
  va_start(argp, fmt);
  msg = luaO_pushvfstring(L, fmt, argp);  /* format message */
  va_end(argp);
  if (isLua(ci)) {  /* if Lua function, add source:line information */
    luaG_addinfo(L, msg, ci_func(ci)->p->source, getcurrentline(ci));
    setobjs2s(L, L->top - 2, L->top - 1);  /* remove 'msg' from the stack */
    L->top--;
  }
  luaG_errormsg(L);
}