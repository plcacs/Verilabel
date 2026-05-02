Expr *sqlite3CreateColumnExpr(sqlite3 *db, SrcList *pSrc, int iSrc, int iCol){
  Expr *p = sqlite3ExprAlloc(db, TK_COLUMN, 0, 0);
  if( p ){
    struct SrcList_item *pItem = &pSrc->a[iSrc];
    Table *pTab = p->y.pTab = pItem->pTab;
    p->iTable = pItem->iCursor;
    if( p->y.pTab->iPKey==iCol ){
      p->iColumn = -1;
    }else{
      p->iColumn = (ynVar)iCol;
      if( pTab->tabFlags & TF_HasGenerated ){
        Column *pColumn = pTab->aCol + iCol;
        if( pColumn->colFlags & COLFLAG_GENERATED ){
          testcase( pTab->nCol==63 );
          testcase( pTab->nCol==64 );
          if( pTab->nCol>=64 ){
            pItem->colUsed = ALLBITS;
          }else{
            pItem->colUsed = MASKBIT(pTab->nCol)-1;
          }
        }
      }else{
        testcase( iCol==BMS );
        testcase( iCol==BMS-1 );
        pItem->colUsed |= ((Bitmask)1)<<(iCol>=BMS ? BMS-1 : iCol);
      }
    }
  }
  return p;
}