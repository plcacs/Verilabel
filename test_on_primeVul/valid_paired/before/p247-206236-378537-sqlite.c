static void vdbeVComment(Vdbe *p, const char *zFormat, va_list ap){
  assert( p->nOp>0 || p->aOp==0 );
  assert( p->aOp==0 || p->aOp[p->nOp-1].zComment==0 || p->db->mallocFailed );
  if( p->nOp ){
    assert( p->aOp );
    sqlite3DbFree(p->db, p->aOp[p->nOp-1].zComment);
    p->aOp[p->nOp-1].zComment = sqlite3VMPrintf(p->db, zFormat, ap);
  }
}