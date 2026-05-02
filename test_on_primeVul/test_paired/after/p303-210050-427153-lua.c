static void singlevar (LexState *ls, expdesc *var) {
  TString *varname = str_checkname(ls);
  FuncState *fs = ls->fs;
  singlevaraux(fs, varname, var, 1);
  if (var->k == VVOID) {  /* global name? */
    expdesc key;
    singlevaraux(fs, ls->envn, var, 1);  /* get environment variable */
    lua_assert(var->k != VVOID);  /* this one must exist */
    luaK_exp2anyregup(fs, var);  /* but could be a constant */
    codestring(&key, varname);  /* key is variable name */
    luaK_indexed(fs, var, &key);  /* env[varname] */
  }
}