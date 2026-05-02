static ptrdiff_t finderrfunc(lua_State *L)
{
  cTValue *frame = L->base-1, *bot = tvref(L->stack);
  void *cf = L->cframe;
  while (frame > bot && cf) {
    while (cframe_nres(cframe_raw(cf)) < 0) {  /* cframe without frame? */
      if (frame >= restorestack(L, -cframe_nres(cf)))
	break;
      if (cframe_errfunc(cf) >= 0)  /* Error handler not inherited (-1)? */
	return cframe_errfunc(cf);
      cf = cframe_prev(cf);  /* Else unwind cframe and continue searching. */
      if (cf == NULL)
	return 0;
    }
    switch (frame_typep(frame)) {
    case FRAME_LUA:
    case FRAME_LUAP:
      frame = frame_prevl(frame);
      break;
    case FRAME_C:
      cf = cframe_prev(cf);
      /* fallthrough */
    case FRAME_VARG:
      frame = frame_prevd(frame);
      break;
    case FRAME_CONT:
#if LJ_HASFFI
      if ((frame-1)->u32.lo == LJ_CONT_FFI_CALLBACK)
	cf = cframe_prev(cf);
#endif
      frame = frame_prevd(frame);
      break;
    case FRAME_CP:
      if (cframe_canyield(cf)) return 0;
      if (cframe_errfunc(cf) >= 0)
	return cframe_errfunc(cf);
      frame = frame_prevd(frame);
      break;
    case FRAME_PCALL:
    case FRAME_PCALLH:
      if (frame_ftsz(frame) >= (ptrdiff_t)(2*sizeof(TValue)))  /* xpcall? */
	return savestack(L, frame-1);  /* Point to xpcall's errorfunc. */
      return 0;
    default:
      lua_assert(0);
      return 0;
    }
  }
  return 0;
}