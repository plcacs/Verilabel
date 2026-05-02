void luaD_callnoyield (lua_State *L, StkId func, int nResults) {
  incXCcalls(L);
  if (getCcalls(L) <= CSTACKERR) {  /* possible C stack overflow? */
    luaE_exitCcall(L);  /* to compensate decrement in next call */
    luaE_enterCcall(L);  /* check properly */
  }
  luaD_call(L, func, nResults);
  decXCcalls(L);
}