int sqlite3Select(
  Parse *pParse,         /* The parser context */
  Select *p,             /* The SELECT statement being coded. */
  SelectDest *pDest      /* What to do with the query results */
){
  int i, j;              /* Loop counters */
  WhereInfo *pWInfo;     /* Return from sqlite3WhereBegin() */
  Vdbe *v;               /* The virtual machine under construction */
  int isAgg;             /* True for select lists like "count(*)" */
  ExprList *pEList = 0;  /* List of columns to extract. */
  SrcList *pTabList;     /* List of tables to select from */
  Expr *pWhere;          /* The WHERE clause.  May be NULL */
  ExprList *pGroupBy;    /* The GROUP BY clause.  May be NULL */
  Expr *pHaving;         /* The HAVING clause.  May be NULL */
  int rc = 1;            /* Value to return from this function */
  DistinctCtx sDistinct; /* Info on how to code the DISTINCT keyword */
  SortCtx sSort;         /* Info on how to code the ORDER BY clause */
  AggInfo sAggInfo;      /* Information used by aggregate queries */
  int iEnd;              /* Address of the end of the query */
  sqlite3 *db;           /* The database connection */
  ExprList *pMinMaxOrderBy = 0;  /* Added ORDER BY for min/max queries */
  u8 minMaxFlag;                 /* Flag for min/max queries */

  db = pParse->db;
  v = sqlite3GetVdbe(pParse);
  if( p==0 || db->mallocFailed || pParse->nErr ){
    return 1;
  }
  if( sqlite3AuthCheck(pParse, SQLITE_SELECT, 0, 0, 0) ) return 1;
  memset(&sAggInfo, 0, sizeof(sAggInfo));
#if SELECTTRACE_ENABLED
  SELECTTRACE(1,pParse,p, ("begin processing:\n", pParse->addrExplain));
  if( sqlite3SelectTrace & 0x100 ){
    sqlite3TreeViewSelect(0, p, 0);
  }
#endif

  assert( p->pOrderBy==0 || pDest->eDest!=SRT_DistFifo );
  assert( p->pOrderBy==0 || pDest->eDest!=SRT_Fifo );
  assert( p->pOrderBy==0 || pDest->eDest!=SRT_DistQueue );
  assert( p->pOrderBy==0 || pDest->eDest!=SRT_Queue );
  if( IgnorableOrderby(pDest) ){
    assert(pDest->eDest==SRT_Exists || pDest->eDest==SRT_Union || 
           pDest->eDest==SRT_Except || pDest->eDest==SRT_Discard ||
           pDest->eDest==SRT_Queue  || pDest->eDest==SRT_DistFifo ||
           pDest->eDest==SRT_DistQueue || pDest->eDest==SRT_Fifo);
    /* If ORDER BY makes no difference in the output then neither does
    ** DISTINCT so it can be removed too. */
    sqlite3ExprListDelete(db, p->pOrderBy);
    p->pOrderBy = 0;
    p->selFlags &= ~SF_Distinct;
  }
  sqlite3SelectPrep(pParse, p, 0);
  if( pParse->nErr || db->mallocFailed ){
    goto select_end;
  }
  assert( p->pEList!=0 );
#if SELECTTRACE_ENABLED
  if( sqlite3SelectTrace & 0x104 ){
    SELECTTRACE(0x104,pParse,p, ("after name resolution:\n"));
    sqlite3TreeViewSelect(0, p, 0);
  }
#endif

  if( pDest->eDest==SRT_Output ){
    generateColumnNames(pParse, p);
  }

#ifndef SQLITE_OMIT_WINDOWFUNC
  if( sqlite3WindowRewrite(pParse, p) ){
    goto select_end;
  }
#if SELECTTRACE_ENABLED
  if( p->pWin && (sqlite3SelectTrace & 0x108)!=0 ){
    SELECTTRACE(0x104,pParse,p, ("after window rewrite:\n"));
    sqlite3TreeViewSelect(0, p, 0);
  }
#endif
#endif /* SQLITE_OMIT_WINDOWFUNC */
  pTabList = p->pSrc;
  isAgg = (p->selFlags & SF_Aggregate)!=0;
  memset(&sSort, 0, sizeof(sSort));
  sSort.pOrderBy = p->pOrderBy;

  /* Try to various optimizations (flattening subqueries, and strength
  ** reduction of join operators) in the FROM clause up into the main query
  */
#if !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW)
  for(i=0; !p->pPrior && i<pTabList->nSrc; i++){
    struct SrcList_item *pItem = &pTabList->a[i];
    Select *pSub = pItem->pSelect;
    Table *pTab = pItem->pTab;

    /* Convert LEFT JOIN into JOIN if there are terms of the right table
    ** of the LEFT JOIN used in the WHERE clause.
    */
    if( (pItem->fg.jointype & JT_LEFT)!=0
     && sqlite3ExprImpliesNonNullRow(p->pWhere, pItem->iCursor)
     && OptimizationEnabled(db, SQLITE_SimplifyJoin)
    ){
      SELECTTRACE(0x100,pParse,p,
                ("LEFT-JOIN simplifies to JOIN on term %d\n",i));
      pItem->fg.jointype &= ~(JT_LEFT|JT_OUTER);
      unsetJoinExpr(p->pWhere, pItem->iCursor);
    }

    /* No futher action if this term of the FROM clause is no a subquery */
    if( pSub==0 ) continue;

    /* Catch mismatch in the declared columns of a view and the number of
    ** columns in the SELECT on the RHS */
    if( pTab->nCol!=pSub->pEList->nExpr ){
      sqlite3ErrorMsg(pParse, "expected %d columns for '%s' but got %d",
                      pTab->nCol, pTab->zName, pSub->pEList->nExpr);
      goto select_end;
    }

    /* Do not try to flatten an aggregate subquery.
    **
    ** Flattening an aggregate subquery is only possible if the outer query
    ** is not a join.  But if the outer query is not a join, then the subquery
    ** will be implemented as a co-routine and there is no advantage to
    ** flattening in that case.
    */
    if( (pSub->selFlags & SF_Aggregate)!=0 ) continue;
    assert( pSub->pGroupBy==0 );

    /* If the outer query contains a "complex" result set (that is,
    ** if the result set of the outer query uses functions or subqueries)
    ** and if the subquery contains an ORDER BY clause and if
    ** it will be implemented as a co-routine, then do not flatten.  This
    ** restriction allows SQL constructs like this:
    **
    **  SELECT expensive_function(x)
    **    FROM (SELECT x FROM tab ORDER BY y LIMIT 10);
    **
    ** The expensive_function() is only computed on the 10 rows that
    ** are output, rather than every row of the table.
    **
    ** The requirement that the outer query have a complex result set
    ** means that flattening does occur on simpler SQL constraints without
    ** the expensive_function() like:
    **
    **  SELECT x FROM (SELECT x FROM tab ORDER BY y LIMIT 10);
    */
    if( pSub->pOrderBy!=0
     && i==0
     && (p->selFlags & SF_ComplexResult)!=0
     && (pTabList->nSrc==1
         || (pTabList->a[1].fg.jointype&(JT_LEFT|JT_CROSS))!=0)
    ){
      continue;
    }

    if( flattenSubquery(pParse, p, i, isAgg) ){
      if( pParse->nErr ) goto select_end;
      /* This subquery can be absorbed into its parent. */
      i = -1;
    }
    pTabList = p->pSrc;
    if( db->mallocFailed ) goto select_end;
    if( !IgnorableOrderby(pDest) ){
      sSort.pOrderBy = p->pOrderBy;
    }
  }
#endif

#ifndef SQLITE_OMIT_COMPOUND_SELECT
  /* Handle compound SELECT statements using the separate multiSelect()
  ** procedure.
  */
  if( p->pPrior ){
    rc = multiSelect(pParse, p, pDest);
#if SELECTTRACE_ENABLED
    SELECTTRACE(0x1,pParse,p,("end compound-select processing\n"));
    if( (sqlite3SelectTrace & 0x2000)!=0 && ExplainQueryPlanParent(pParse)==0 ){
      sqlite3TreeViewSelect(0, p, 0);
    }
#endif
    if( p->pNext==0 ) ExplainQueryPlanPop(pParse);
    return rc;
  }
#endif

  /* Do the WHERE-clause constant propagation optimization if this is
  ** a join.  No need to speed time on this operation for non-join queries
  ** as the equivalent optimization will be handled by query planner in
  ** sqlite3WhereBegin().
  */
  if( pTabList->nSrc>1
   && OptimizationEnabled(db, SQLITE_PropagateConst)
   && propagateConstants(pParse, p)
  ){
#if SELECTTRACE_ENABLED
    if( sqlite3SelectTrace & 0x100 ){
      SELECTTRACE(0x100,pParse,p,("After constant propagation:\n"));
      sqlite3TreeViewSelect(0, p, 0);
    }
#endif
  }else{
    SELECTTRACE(0x100,pParse,p,("Constant propagation not helpful\n"));
  }

#ifdef SQLITE_COUNTOFVIEW_OPTIMIZATION
  if( OptimizationEnabled(db, SQLITE_QueryFlattener|SQLITE_CountOfView)
   && countOfViewOptimization(pParse, p)
  ){
    if( db->mallocFailed ) goto select_end;
    pEList = p->pEList;
    pTabList = p->pSrc;
  }
#endif

  /* For each term in the FROM clause, do two things:
  ** (1) Authorized unreferenced tables
  ** (2) Generate code for all sub-queries
  */
  for(i=0; i<pTabList->nSrc; i++){
    struct SrcList_item *pItem = &pTabList->a[i];
    SelectDest dest;
    Select *pSub;
#if !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW)
    const char *zSavedAuthContext;
#endif

    /* Issue SQLITE_READ authorizations with a fake column name for any
    ** tables that are referenced but from which no values are extracted.
    ** Examples of where these kinds of null SQLITE_READ authorizations
    ** would occur:
    **
    **     SELECT count(*) FROM t1;   -- SQLITE_READ t1.""
    **     SELECT t1.* FROM t1, t2;   -- SQLITE_READ t2.""
    **
    ** The fake column name is an empty string.  It is possible for a table to
    ** have a column named by the empty string, in which case there is no way to
    ** distinguish between an unreferenced table and an actual reference to the
    ** "" column. The original design was for the fake column name to be a NULL,
    ** which would be unambiguous.  But legacy authorization callbacks might
    ** assume the column name is non-NULL and segfault.  The use of an empty
    ** string for the fake column name seems safer.
    */
    if( pItem->colUsed==0 && pItem->zName!=0 ){
      sqlite3AuthCheck(pParse, SQLITE_READ, pItem->zName, "", pItem->zDatabase);
    }

#if !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW)
    /* Generate code for all sub-queries in the FROM clause
    */
    pSub = pItem->pSelect;
    if( pSub==0 ) continue;

    /* The code for a subquery should only be generated once, though it is
    ** technically harmless for it to be generated multiple times. The
    ** following assert() will detect if something changes to cause
    ** the same subquery to be coded multiple times, as a signal to the
    ** developers to try to optimize the situation.
    **
    ** Update 2019-07-24:
    ** See ticket https://sqlite.org/src/tktview/c52b09c7f38903b1311cec40.
    ** The dbsqlfuzz fuzzer found a case where the same subquery gets
    ** coded twice.  So this assert() now becomes a testcase().  It should
    ** be very rare, though.
    */
    testcase( pItem->addrFillSub!=0 );

    /* Increment Parse.nHeight by the height of the largest expression
    ** tree referred to by this, the parent select. The child select
    ** may contain expression trees of at most
    ** (SQLITE_MAX_EXPR_DEPTH-Parse.nHeight) height. This is a bit
    ** more conservative than necessary, but much easier than enforcing
    ** an exact limit.
    */
    pParse->nHeight += sqlite3SelectExprHeight(p);

    /* Make copies of constant WHERE-clause terms in the outer query down
    ** inside the subquery.  This can help the subquery to run more efficiently.
    */
    if( OptimizationEnabled(db, SQLITE_PushDown)
     && pushDownWhereTerms(pParse, pSub, p->pWhere, pItem->iCursor,
                           (pItem->fg.jointype & JT_OUTER)!=0)
    ){
#if SELECTTRACE_ENABLED
      if( sqlite3SelectTrace & 0x100 ){
        SELECTTRACE(0x100,pParse,p,
            ("After WHERE-clause push-down into subquery %d:\n", pSub->selId));
        sqlite3TreeViewSelect(0, p, 0);
      }
#endif
    }else{
      SELECTTRACE(0x100,pParse,p,("Push-down not possible\n"));
    }

    zSavedAuthContext = pParse->zAuthContext;
    pParse->zAuthContext = pItem->zName;

    /* Generate code to implement the subquery
    **
    ** The subquery is implemented as a co-routine if the subquery is
    ** guaranteed to be the outer loop (so that it does not need to be
    ** computed more than once)
    **
    ** TODO: Are there other reasons beside (1) to use a co-routine
    ** implementation?
    */
    if( i==0
     && (pTabList->nSrc==1
            || (pTabList->a[1].fg.jointype&(JT_LEFT|JT_CROSS))!=0)  /* (1) */
    ){
      /* Implement a co-routine that will return a single row of the result
      ** set on each invocation.
      */
      int addrTop = sqlite3VdbeCurrentAddr(v)+1;
     
      pItem->regReturn = ++pParse->nMem;
      sqlite3VdbeAddOp3(v, OP_InitCoroutine, pItem->regReturn, 0, addrTop);
      VdbeComment((v, "%s", pItem->pTab->zName));
      pItem->addrFillSub = addrTop;
      sqlite3SelectDestInit(&dest, SRT_Coroutine, pItem->regReturn);
      ExplainQueryPlan((pParse, 1, "CO-ROUTINE %u", pSub->selId));
      sqlite3Select(pParse, pSub, &dest);
      pItem->pTab->nRowLogEst = pSub->nSelectRow;
      pItem->fg.viaCoroutine = 1;
      pItem->regResult = dest.iSdst;
      sqlite3VdbeEndCoroutine(v, pItem->regReturn);
      sqlite3VdbeJumpHere(v, addrTop-1);
      sqlite3ClearTempRegCache(pParse);
    }else{
      /* Generate a subroutine that will fill an ephemeral table with
      ** the content of this subquery.  pItem->addrFillSub will point
      ** to the address of the generated subroutine.  pItem->regReturn
      ** is a register allocated to hold the subroutine return address
      */
      int topAddr;
      int onceAddr = 0;
      int retAddr;
      struct SrcList_item *pPrior;

      testcase( pItem->addrFillSub==0 ); /* Ticket c52b09c7f38903b1311 */
      pItem->regReturn = ++pParse->nMem;
      topAddr = sqlite3VdbeAddOp2(v, OP_Integer, 0, pItem->regReturn);
      pItem->addrFillSub = topAddr+1;
      if( pItem->fg.isCorrelated==0 ){
        /* If the subquery is not correlated and if we are not inside of
        ** a trigger, then we only need to compute the value of the subquery
        ** once. */
        onceAddr = sqlite3VdbeAddOp0(v, OP_Once); VdbeCoverage(v);
        VdbeComment((v, "materialize \"%s\"", pItem->pTab->zName));
      }else{
        VdbeNoopComment((v, "materialize \"%s\"", pItem->pTab->zName));
      }
      pPrior = isSelfJoinView(pTabList, pItem);
      if( pPrior ){
        sqlite3VdbeAddOp2(v, OP_OpenDup, pItem->iCursor, pPrior->iCursor);
        assert( pPrior->pSelect!=0 );
        pSub->nSelectRow = pPrior->pSelect->nSelectRow;
      }else{
        sqlite3SelectDestInit(&dest, SRT_EphemTab, pItem->iCursor);
        ExplainQueryPlan((pParse, 1, "MATERIALIZE %u", pSub->selId));
        sqlite3Select(pParse, pSub, &dest);
      }
      pItem->pTab->nRowLogEst = pSub->nSelectRow;
      if( onceAddr ) sqlite3VdbeJumpHere(v, onceAddr);
      retAddr = sqlite3VdbeAddOp1(v, OP_Return, pItem->regReturn);
      VdbeComment((v, "end %s", pItem->pTab->zName));
      sqlite3VdbeChangeP1(v, topAddr, retAddr);
      sqlite3ClearTempRegCache(pParse);
    }
    if( db->mallocFailed ) goto select_end;
    pParse->nHeight -= sqlite3SelectExprHeight(p);
    pParse->zAuthContext = zSavedAuthContext;
#endif
  }

  /* Various elements of the SELECT copied into local variables for
  ** convenience */
  pEList = p->pEList;
  pWhere = p->pWhere;
  pGroupBy = p->pGroupBy;
  pHaving = p->pHaving;
  sDistinct.isTnct = (p->selFlags & SF_Distinct)!=0;

#if SELECTTRACE_ENABLED
  if( sqlite3SelectTrace & 0x400 ){
    SELECTTRACE(0x400,pParse,p,("After all FROM-clause analysis:\n"));
    sqlite3TreeViewSelect(0, p, 0);
  }
#endif

  /* If the query is DISTINCT with an ORDER BY but is not an aggregate, and 
  ** if the select-list is the same as the ORDER BY list, then this query
  ** can be rewritten as a GROUP BY. In other words, this:
  **
  **     SELECT DISTINCT xyz FROM ... ORDER BY xyz
  **
  ** is transformed to:
  **
  **     SELECT xyz FROM ... GROUP BY xyz ORDER BY xyz
  **
  ** The second form is preferred as a single index (or temp-table) may be 
  ** used for both the ORDER BY and DISTINCT processing. As originally 
  ** written the query must use a temp-table for at least one of the ORDER 
  ** BY and DISTINCT, and an index or separate temp-table for the other.
  */
  if( (p->selFlags & (SF_Distinct|SF_Aggregate))==SF_Distinct 
   && sqlite3ExprListCompare(sSort.pOrderBy, pEList, -1)==0
  ){
    p->selFlags &= ~SF_Distinct;
    pGroupBy = p->pGroupBy = sqlite3ExprListDup(db, pEList, 0);
    /* Notice that even thought SF_Distinct has been cleared from p->selFlags,
    ** the sDistinct.isTnct is still set.  Hence, isTnct represents the
    ** original setting of the SF_Distinct flag, not the current setting */
    assert( sDistinct.isTnct );

#if SELECTTRACE_ENABLED
    if( sqlite3SelectTrace & 0x400 ){
      SELECTTRACE(0x400,pParse,p,("Transform DISTINCT into GROUP BY:\n"));
      sqlite3TreeViewSelect(0, p, 0);
    }
#endif
  }

  /* If there is an ORDER BY clause, then create an ephemeral index to
  ** do the sorting.  But this sorting ephemeral index might end up
  ** being unused if the data can be extracted in pre-sorted order.
  ** If that is the case, then the OP_OpenEphemeral instruction will be
  ** changed to an OP_Noop once we figure out that the sorting index is
  ** not needed.  The sSort.addrSortIndex variable is used to facilitate
  ** that change.
  */
  if( sSort.pOrderBy ){
    KeyInfo *pKeyInfo;
    pKeyInfo = sqlite3KeyInfoFromExprList(
        pParse, sSort.pOrderBy, 0, pEList->nExpr);
    sSort.iECursor = pParse->nTab++;
    sSort.addrSortIndex =
      sqlite3VdbeAddOp4(v, OP_OpenEphemeral,
          sSort.iECursor, sSort.pOrderBy->nExpr+1+pEList->nExpr, 0,
          (char*)pKeyInfo, P4_KEYINFO
      );
  }else{
    sSort.addrSortIndex = -1;
  }

  /* If the output is destined for a temporary table, open that table.
  */
  if( pDest->eDest==SRT_EphemTab ){
    sqlite3VdbeAddOp2(v, OP_OpenEphemeral, pDest->iSDParm, pEList->nExpr);
  }

  /* Set the limiter.
  */
  iEnd = sqlite3VdbeMakeLabel(pParse);
  if( (p->selFlags & SF_FixedLimit)==0 ){
    p->nSelectRow = 320;  /* 4 billion rows */
  }
  computeLimitRegisters(pParse, p, iEnd);
  if( p->iLimit==0 && sSort.addrSortIndex>=0 ){
    sqlite3VdbeChangeOpcode(v, sSort.addrSortIndex, OP_SorterOpen);
    sSort.sortFlags |= SORTFLAG_UseSorter;
  }

  /* Open an ephemeral index to use for the distinct set.
  */
  if( p->selFlags & SF_Distinct ){
    sDistinct.tabTnct = pParse->nTab++;
    sDistinct.addrTnct = sqlite3VdbeAddOp4(v, OP_OpenEphemeral,
                       sDistinct.tabTnct, 0, 0,
                       (char*)sqlite3KeyInfoFromExprList(pParse, p->pEList,0,0),
                       P4_KEYINFO);
    sqlite3VdbeChangeP5(v, BTREE_UNORDERED);
    sDistinct.eTnctType = WHERE_DISTINCT_UNORDERED;
  }else{
    sDistinct.eTnctType = WHERE_DISTINCT_NOOP;
  }

  if( !isAgg && pGroupBy==0 ){
    /* No aggregate functions and no GROUP BY clause */
    u16 wctrlFlags = (sDistinct.isTnct ? WHERE_WANT_DISTINCT : 0)
                   | (p->selFlags & SF_FixedLimit);
#ifndef SQLITE_OMIT_WINDOWFUNC
    Window *pWin = p->pWin;      /* Master window object (or NULL) */
    if( pWin ){
      sqlite3WindowCodeInit(pParse, pWin);
    }
#endif
    assert( WHERE_USE_LIMIT==SF_FixedLimit );


    /* Begin the database scan. */
    SELECTTRACE(1,pParse,p,("WhereBegin\n"));
    pWInfo = sqlite3WhereBegin(pParse, pTabList, pWhere, sSort.pOrderBy,
                               p->pEList, wctrlFlags, p->nSelectRow);
    if( pWInfo==0 ) goto select_end;
    if( sqlite3WhereOutputRowCount(pWInfo) < p->nSelectRow ){
      p->nSelectRow = sqlite3WhereOutputRowCount(pWInfo);
    }
    if( sDistinct.isTnct && sqlite3WhereIsDistinct(pWInfo) ){
      sDistinct.eTnctType = sqlite3WhereIsDistinct(pWInfo);
    }
    if( sSort.pOrderBy ){
      sSort.nOBSat = sqlite3WhereIsOrdered(pWInfo);
      sSort.labelOBLopt = sqlite3WhereOrderByLimitOptLabel(pWInfo);
      if( sSort.nOBSat==sSort.pOrderBy->nExpr ){
        sSort.pOrderBy = 0;
      }
    }

    /* If sorting index that was created by a prior OP_OpenEphemeral 
    ** instruction ended up not being needed, then change the OP_OpenEphemeral
    ** into an OP_Noop.
    */
    if( sSort.addrSortIndex>=0 && sSort.pOrderBy==0 ){
      sqlite3VdbeChangeToNoop(v, sSort.addrSortIndex);
    }

    assert( p->pEList==pEList );
#ifndef SQLITE_OMIT_WINDOWFUNC
    if( pWin ){
      int addrGosub = sqlite3VdbeMakeLabel(pParse);
      int iCont = sqlite3VdbeMakeLabel(pParse);
      int iBreak = sqlite3VdbeMakeLabel(pParse);
      int regGosub = ++pParse->nMem;

      sqlite3WindowCodeStep(pParse, p, pWInfo, regGosub, addrGosub);

      sqlite3VdbeAddOp2(v, OP_Goto, 0, iBreak);
      sqlite3VdbeResolveLabel(v, addrGosub);
      VdbeNoopComment((v, "inner-loop subroutine"));
      sSort.labelOBLopt = 0;
      selectInnerLoop(pParse, p, -1, &sSort, &sDistinct, pDest, iCont, iBreak);
      sqlite3VdbeResolveLabel(v, iCont);
      sqlite3VdbeAddOp1(v, OP_Return, regGosub);
      VdbeComment((v, "end inner-loop subroutine"));
      sqlite3VdbeResolveLabel(v, iBreak);
    }else
#endif /* SQLITE_OMIT_WINDOWFUNC */
    {
      /* Use the standard inner loop. */
      selectInnerLoop(pParse, p, -1, &sSort, &sDistinct, pDest,
          sqlite3WhereContinueLabel(pWInfo),
          sqlite3WhereBreakLabel(pWInfo));

      /* End the database scan loop.
      */
      sqlite3WhereEnd(pWInfo);
    }
  }else{
    /* This case when there exist aggregate functions or a GROUP BY clause
    ** or both */
    NameContext sNC;    /* Name context for processing aggregate information */
    int iAMem;          /* First Mem address for storing current GROUP BY */
    int iBMem;          /* First Mem address for previous GROUP BY */
    int iUseFlag;       /* Mem address holding flag indicating that at least
                        ** one row of the input to the aggregator has been
                        ** processed */
    int iAbortFlag;     /* Mem address which causes query abort if positive */
    int groupBySort;    /* Rows come from source in GROUP BY order */
    int addrEnd;        /* End of processing for this SELECT */
    int sortPTab = 0;   /* Pseudotable used to decode sorting results */
    int sortOut = 0;    /* Output register from the sorter */
    int orderByGrp = 0; /* True if the GROUP BY and ORDER BY are the same */

    /* Remove any and all aliases between the result set and the
    ** GROUP BY clause.
    */
    if( pGroupBy ){
      int k;                        /* Loop counter */
      struct ExprList_item *pItem;  /* For looping over expression in a list */

      for(k=p->pEList->nExpr, pItem=p->pEList->a; k>0; k--, pItem++){
        pItem->u.x.iAlias = 0;
      }
      for(k=pGroupBy->nExpr, pItem=pGroupBy->a; k>0; k--, pItem++){
        pItem->u.x.iAlias = 0;
      }
      assert( 66==sqlite3LogEst(100) );
      if( p->nSelectRow>66 ) p->nSelectRow = 66;

      /* If there is both a GROUP BY and an ORDER BY clause and they are
      ** identical, then it may be possible to disable the ORDER BY clause 
      ** on the grounds that the GROUP BY will cause elements to come out 
      ** in the correct order. It also may not - the GROUP BY might use a
      ** database index that causes rows to be grouped together as required
      ** but not actually sorted. Either way, record the fact that the
      ** ORDER BY and GROUP BY clauses are the same by setting the orderByGrp
      ** variable.  */
      if( sSort.pOrderBy && pGroupBy->nExpr==sSort.pOrderBy->nExpr ){
        int ii;
        /* The GROUP BY processing doesn't care whether rows are delivered in
        ** ASC or DESC order - only that each group is returned contiguously.
        ** So set the ASC/DESC flags in the GROUP BY to match those in the 
        ** ORDER BY to maximize the chances of rows being delivered in an 
        ** order that makes the ORDER BY redundant.  */
        for(ii=0; ii<pGroupBy->nExpr; ii++){
          u8 sortFlags = sSort.pOrderBy->a[ii].sortFlags & KEYINFO_ORDER_DESC;
          pGroupBy->a[ii].sortFlags = sortFlags;
        }
        if( sqlite3ExprListCompare(pGroupBy, sSort.pOrderBy, -1)==0 ){
          orderByGrp = 1;
        }
      }
    }else{
      assert( 0==sqlite3LogEst(1) );
      p->nSelectRow = 0;
    }

    /* Create a label to jump to when we want to abort the query */
    addrEnd = sqlite3VdbeMakeLabel(pParse);

    /* Convert TK_COLUMN nodes into TK_AGG_COLUMN and make entries in
    ** sAggInfo for all TK_AGG_FUNCTION nodes in expressions of the
    ** SELECT statement.
    */
    memset(&sNC, 0, sizeof(sNC));
    sNC.pParse = pParse;
    sNC.pSrcList = pTabList;
    sNC.uNC.pAggInfo = &sAggInfo;
    VVA_ONLY( sNC.ncFlags = NC_UAggInfo; )
    sAggInfo.mnReg = pParse->nMem+1;
    sAggInfo.nSortingColumn = pGroupBy ? pGroupBy->nExpr : 0;
    sAggInfo.pGroupBy = pGroupBy;
    sqlite3ExprAnalyzeAggList(&sNC, pEList);
    sqlite3ExprAnalyzeAggList(&sNC, sSort.pOrderBy);
    if( pHaving ){
      if( pGroupBy ){
        assert( pWhere==p->pWhere );
        assert( pHaving==p->pHaving );
        assert( pGroupBy==p->pGroupBy );
        havingToWhere(pParse, p);
        pWhere = p->pWhere;
      }
      sqlite3ExprAnalyzeAggregates(&sNC, pHaving);
    }
    sAggInfo.nAccumulator = sAggInfo.nColumn;
    if( p->pGroupBy==0 && p->pHaving==0 && sAggInfo.nFunc==1 ){
      minMaxFlag = minMaxQuery(db, sAggInfo.aFunc[0].pExpr, &pMinMaxOrderBy);
    }else{
      minMaxFlag = WHERE_ORDERBY_NORMAL;
    }
    for(i=0; i<sAggInfo.nFunc; i++){
      Expr *pExpr = sAggInfo.aFunc[i].pExpr;
      assert( !ExprHasProperty(pExpr, EP_xIsSelect) );
      sNC.ncFlags |= NC_InAggFunc;
      sqlite3ExprAnalyzeAggList(&sNC, pExpr->x.pList);
#ifndef SQLITE_OMIT_WINDOWFUNC
      assert( !IsWindowFunc(pExpr) );
      if( ExprHasProperty(pExpr, EP_WinFunc) ){
        sqlite3ExprAnalyzeAggregates(&sNC, pExpr->y.pWin->pFilter);
      }
#endif
      sNC.ncFlags &= ~NC_InAggFunc;
    }
    sAggInfo.mxReg = pParse->nMem;
    if( db->mallocFailed ) goto select_end;
#if SELECTTRACE_ENABLED
    if( sqlite3SelectTrace & 0x400 ){
      int ii;
      SELECTTRACE(0x400,pParse,p,("After aggregate analysis:\n"));
      sqlite3TreeViewSelect(0, p, 0);
      for(ii=0; ii<sAggInfo.nColumn; ii++){
        sqlite3DebugPrintf("agg-column[%d] iMem=%d\n",
            ii, sAggInfo.aCol[ii].iMem);
        sqlite3TreeViewExpr(0, sAggInfo.aCol[ii].pExpr, 0);
      }
      for(ii=0; ii<sAggInfo.nFunc; ii++){
        sqlite3DebugPrintf("agg-func[%d]: iMem=%d\n",
            ii, sAggInfo.aFunc[ii].iMem);
        sqlite3TreeViewExpr(0, sAggInfo.aFunc[ii].pExpr, 0);
      }
    }
#endif


    /* Processing for aggregates with GROUP BY is very different and
    ** much more complex than aggregates without a GROUP BY.
    */
    if( pGroupBy ){
      KeyInfo *pKeyInfo;  /* Keying information for the group by clause */
      int addr1;          /* A-vs-B comparision jump */
      int addrOutputRow;  /* Start of subroutine that outputs a result row */
      int regOutputRow;   /* Return address register for output subroutine */
      int addrSetAbort;   /* Set the abort flag and return */
      int addrTopOfLoop;  /* Top of the input loop */
      int addrSortingIdx; /* The OP_OpenEphemeral for the sorting index */
      int addrReset;      /* Subroutine for resetting the accumulator */
      int regReset;       /* Return address register for reset subroutine */

      /* If there is a GROUP BY clause we might need a sorting index to
      ** implement it.  Allocate that sorting index now.  If it turns out
      ** that we do not need it after all, the OP_SorterOpen instruction
      ** will be converted into a Noop.  
      */
      sAggInfo.sortingIdx = pParse->nTab++;
      pKeyInfo = sqlite3KeyInfoFromExprList(pParse,pGroupBy,0,sAggInfo.nColumn);
      addrSortingIdx = sqlite3VdbeAddOp4(v, OP_SorterOpen, 
          sAggInfo.sortingIdx, sAggInfo.nSortingColumn, 
          0, (char*)pKeyInfo, P4_KEYINFO);

      /* Initialize memory locations used by GROUP BY aggregate processing
      */
      iUseFlag = ++pParse->nMem;
      iAbortFlag = ++pParse->nMem;
      regOutputRow = ++pParse->nMem;
      addrOutputRow = sqlite3VdbeMakeLabel(pParse);
      regReset = ++pParse->nMem;
      addrReset = sqlite3VdbeMakeLabel(pParse);
      iAMem = pParse->nMem + 1;
      pParse->nMem += pGroupBy->nExpr;
      iBMem = pParse->nMem + 1;
      pParse->nMem += pGroupBy->nExpr;
      sqlite3VdbeAddOp2(v, OP_Integer, 0, iAbortFlag);
      VdbeComment((v, "clear abort flag"));
      sqlite3VdbeAddOp3(v, OP_Null, 0, iAMem, iAMem+pGroupBy->nExpr-1);

      /* Begin a loop that will extract all source rows in GROUP BY order.
      ** This might involve two separate loops with an OP_Sort in between, or
      ** it might be a single loop that uses an index to extract information
      ** in the right order to begin with.
      */
      sqlite3VdbeAddOp2(v, OP_Gosub, regReset, addrReset);
      SELECTTRACE(1,pParse,p,("WhereBegin\n"));
      pWInfo = sqlite3WhereBegin(pParse, pTabList, pWhere, pGroupBy, 0,
          WHERE_GROUPBY | (orderByGrp ? WHERE_SORTBYGROUP : 0), 0
      );
      if( pWInfo==0 ) goto select_end;
      if( sqlite3WhereIsOrdered(pWInfo)==pGroupBy->nExpr ){
        /* The optimizer is able to deliver rows in group by order so
        ** we do not have to sort.  The OP_OpenEphemeral table will be
        ** cancelled later because we still need to use the pKeyInfo
        */
        groupBySort = 0;
      }else{
        /* Rows are coming out in undetermined order.  We have to push
        ** each row into a sorting index, terminate the first loop,
        ** then loop over the sorting index in order to get the output
        ** in sorted order
        */
        int regBase;
        int regRecord;
        int nCol;
        int nGroupBy;

        explainTempTable(pParse, 
            (sDistinct.isTnct && (p->selFlags&SF_Distinct)==0) ?
                    "DISTINCT" : "GROUP BY");

        groupBySort = 1;
        nGroupBy = pGroupBy->nExpr;
        nCol = nGroupBy;
        j = nGroupBy;
        for(i=0; i<sAggInfo.nColumn; i++){
          if( sAggInfo.aCol[i].iSorterColumn>=j ){
            nCol++;
            j++;
          }
        }
        regBase = sqlite3GetTempRange(pParse, nCol);
        sqlite3ExprCodeExprList(pParse, pGroupBy, regBase, 0, 0);
        j = nGroupBy;
        for(i=0; i<sAggInfo.nColumn; i++){
          struct AggInfo_col *pCol = &sAggInfo.aCol[i];
          if( pCol->iSorterColumn>=j ){
            int r1 = j + regBase;
            sqlite3ExprCodeGetColumnOfTable(v,
                               pCol->pTab, pCol->iTable, pCol->iColumn, r1);
            j++;
          }
        }
        regRecord = sqlite3GetTempReg(pParse);
        sqlite3VdbeAddOp3(v, OP_MakeRecord, regBase, nCol, regRecord);
        sqlite3VdbeAddOp2(v, OP_SorterInsert, sAggInfo.sortingIdx, regRecord);
        sqlite3ReleaseTempReg(pParse, regRecord);
        sqlite3ReleaseTempRange(pParse, regBase, nCol);
        sqlite3WhereEnd(pWInfo);
        sAggInfo.sortingIdxPTab = sortPTab = pParse->nTab++;
        sortOut = sqlite3GetTempReg(pParse);
        sqlite3VdbeAddOp3(v, OP_OpenPseudo, sortPTab, sortOut, nCol);
        sqlite3VdbeAddOp2(v, OP_SorterSort, sAggInfo.sortingIdx, addrEnd);
        VdbeComment((v, "GROUP BY sort")); VdbeCoverage(v);
        sAggInfo.useSortingIdx = 1;
      }

      /* If the index or temporary table used by the GROUP BY sort
      ** will naturally deliver rows in the order required by the ORDER BY
      ** clause, cancel the ephemeral table open coded earlier.
      **
      ** This is an optimization - the correct answer should result regardless.
      ** Use the SQLITE_GroupByOrder flag with SQLITE_TESTCTRL_OPTIMIZER to 
      ** disable this optimization for testing purposes.  */
      if( orderByGrp && OptimizationEnabled(db, SQLITE_GroupByOrder) 
       && (groupBySort || sqlite3WhereIsSorted(pWInfo))
      ){
        sSort.pOrderBy = 0;
        sqlite3VdbeChangeToNoop(v, sSort.addrSortIndex);
      }

      /* Evaluate the current GROUP BY terms and store in b0, b1, b2...
      ** (b0 is memory location iBMem+0, b1 is iBMem+1, and so forth)
      ** Then compare the current GROUP BY terms against the GROUP BY terms
      ** from the previous row currently stored in a0, a1, a2...
      */
      addrTopOfLoop = sqlite3VdbeCurrentAddr(v);
      if( groupBySort ){
        sqlite3VdbeAddOp3(v, OP_SorterData, sAggInfo.sortingIdx,
                          sortOut, sortPTab);
      }
      for(j=0; j<pGroupBy->nExpr; j++){
        if( groupBySort ){
          sqlite3VdbeAddOp3(v, OP_Column, sortPTab, j, iBMem+j);
        }else{
          sAggInfo.directMode = 1;
          sqlite3ExprCode(pParse, pGroupBy->a[j].pExpr, iBMem+j);
        }
      }
      sqlite3VdbeAddOp4(v, OP_Compare, iAMem, iBMem, pGroupBy->nExpr,
                          (char*)sqlite3KeyInfoRef(pKeyInfo), P4_KEYINFO);
      addr1 = sqlite3VdbeCurrentAddr(v);
      sqlite3VdbeAddOp3(v, OP_Jump, addr1+1, 0, addr1+1); VdbeCoverage(v);

      /* Generate code that runs whenever the GROUP BY changes.
      ** Changes in the GROUP BY are detected by the previous code
      ** block.  If there were no changes, this block is skipped.
      **
      ** This code copies current group by terms in b0,b1,b2,...
      ** over to a0,a1,a2.  It then calls the output subroutine
      ** and resets the aggregate accumulator registers in preparation
      ** for the next GROUP BY batch.
      */
      sqlite3ExprCodeMove(pParse, iBMem, iAMem, pGroupBy->nExpr);
      sqlite3VdbeAddOp2(v, OP_Gosub, regOutputRow, addrOutputRow);
      VdbeComment((v, "output one row"));
      sqlite3VdbeAddOp2(v, OP_IfPos, iAbortFlag, addrEnd); VdbeCoverage(v);
      VdbeComment((v, "check abort flag"));
      sqlite3VdbeAddOp2(v, OP_Gosub, regReset, addrReset);
      VdbeComment((v, "reset accumulator"));

      /* Update the aggregate accumulators based on the content of
      ** the current row
      */
      sqlite3VdbeJumpHere(v, addr1);
      updateAccumulator(pParse, iUseFlag, &sAggInfo);
      sqlite3VdbeAddOp2(v, OP_Integer, 1, iUseFlag);
      VdbeComment((v, "indicate data in accumulator"));

      /* End of the loop
      */
      if( groupBySort ){
        sqlite3VdbeAddOp2(v, OP_SorterNext, sAggInfo.sortingIdx, addrTopOfLoop);
        VdbeCoverage(v);
      }else{
        sqlite3WhereEnd(pWInfo);
        sqlite3VdbeChangeToNoop(v, addrSortingIdx);
      }

      /* Output the final row of result
      */
      sqlite3VdbeAddOp2(v, OP_Gosub, regOutputRow, addrOutputRow);
      VdbeComment((v, "output final row"));

      /* Jump over the subroutines
      */
      sqlite3VdbeGoto(v, addrEnd);

      /* Generate a subroutine that outputs a single row of the result
      ** set.  This subroutine first looks at the iUseFlag.  If iUseFlag
      ** is less than or equal to zero, the subroutine is a no-op.  If
      ** the processing calls for the query to abort, this subroutine
      ** increments the iAbortFlag memory location before returning in
      ** order to signal the caller to abort.
      */
      addrSetAbort = sqlite3VdbeCurrentAddr(v);
      sqlite3VdbeAddOp2(v, OP_Integer, 1, iAbortFlag);
      VdbeComment((v, "set abort flag"));
      sqlite3VdbeAddOp1(v, OP_Return, regOutputRow);
      sqlite3VdbeResolveLabel(v, addrOutputRow);
      addrOutputRow = sqlite3VdbeCurrentAddr(v);
      sqlite3VdbeAddOp2(v, OP_IfPos, iUseFlag, addrOutputRow+2);
      VdbeCoverage(v);
      VdbeComment((v, "Groupby result generator entry point"));
      sqlite3VdbeAddOp1(v, OP_Return, regOutputRow);
      finalizeAggFunctions(pParse, &sAggInfo);
      sqlite3ExprIfFalse(pParse, pHaving, addrOutputRow+1, SQLITE_JUMPIFNULL);
      selectInnerLoop(pParse, p, -1, &sSort,
                      &sDistinct, pDest,
                      addrOutputRow+1, addrSetAbort);
      sqlite3VdbeAddOp1(v, OP_Return, regOutputRow);
      VdbeComment((v, "end groupby result generator"));

      /* Generate a subroutine that will reset the group-by accumulator
      */
      sqlite3VdbeResolveLabel(v, addrReset);
      resetAccumulator(pParse, &sAggInfo);
      sqlite3VdbeAddOp2(v, OP_Integer, 0, iUseFlag);
      VdbeComment((v, "indicate accumulator empty"));
      sqlite3VdbeAddOp1(v, OP_Return, regReset);
     
    } /* endif pGroupBy.  Begin aggregate queries without GROUP BY: */
    else {
#ifndef SQLITE_OMIT_BTREECOUNT
      Table *pTab;
      if( (pTab = isSimpleCount(p, &sAggInfo))!=0 ){
        /* If isSimpleCount() returns a pointer to a Table structure, then
        ** the SQL statement is of the form:
        **
        **   SELECT count(*) FROM <tbl>
        **
        ** where the Table structure returned represents table <tbl>.
        **
        ** This statement is so common that it is optimized specially. The
        ** OP_Count instruction is executed either on the intkey table that
        ** contains the data for table <tbl> or on one of its indexes. It
        ** is better to execute the op on an index, as indexes are almost
        ** always spread across less pages than their corresponding tables.
        */
        const int iDb = sqlite3SchemaToIndex(pParse->db, pTab->pSchema);
        const int iCsr = pParse->nTab++;     /* Cursor to scan b-tree */
        Index *pIdx;                         /* Iterator variable */
        KeyInfo *pKeyInfo = 0;               /* Keyinfo for scanned index */
        Index *pBest = 0;                    /* Best index found so far */
        int iRoot = pTab->tnum;              /* Root page of scanned b-tree */

        sqlite3CodeVerifySchema(pParse, iDb);
        sqlite3TableLock(pParse, iDb, pTab->tnum, 0, pTab->zName);

        /* Search for the index that has the lowest scan cost.
        **
        ** (2011-04-15) Do not do a full scan of an unordered index.
        **
        ** (2013-10-03) Do not count the entries in a partial index.
        **
        ** In practice the KeyInfo structure will not be used. It is only 
        ** passed to keep OP_OpenRead happy.
        */
        if( !HasRowid(pTab) ) pBest = sqlite3PrimaryKeyIndex(pTab);
        for(pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext){
          if( pIdx->bUnordered==0
           && pIdx->szIdxRow<pTab->szTabRow
           && pIdx->pPartIdxWhere==0
           && (!pBest || pIdx->szIdxRow<pBest->szIdxRow)
          ){
            pBest = pIdx;
          }
        }
        if( pBest ){
          iRoot = pBest->tnum;
          pKeyInfo = sqlite3KeyInfoOfIndex(pParse, pBest);
        }

        /* Open a read-only cursor, execute the OP_Count, close the cursor. */
        sqlite3VdbeAddOp4Int(v, OP_OpenRead, iCsr, iRoot, iDb, 1);
        if( pKeyInfo ){
          sqlite3VdbeChangeP4(v, -1, (char *)pKeyInfo, P4_KEYINFO);
        }
        sqlite3VdbeAddOp2(v, OP_Count, iCsr, sAggInfo.aFunc[0].iMem);
        sqlite3VdbeAddOp1(v, OP_Close, iCsr);
        explainSimpleCount(pParse, pTab, pBest);
      }else
#endif /* SQLITE_OMIT_BTREECOUNT */
      {
        int regAcc = 0;           /* "populate accumulators" flag */

        /* If there are accumulator registers but no min() or max() functions
        ** without FILTER clauses, allocate register regAcc. Register regAcc
        ** will contain 0 the first time the inner loop runs, and 1 thereafter.
        ** The code generated by updateAccumulator() uses this to ensure
        ** that the accumulator registers are (a) updated only once if
        ** there are no min() or max functions or (b) always updated for the
        ** first row visited by the aggregate, so that they are updated at
        ** least once even if the FILTER clause means the min() or max() 
        ** function visits zero rows.  */
        if( sAggInfo.nAccumulator ){
          for(i=0; i<sAggInfo.nFunc; i++){
            if( ExprHasProperty(sAggInfo.aFunc[i].pExpr, EP_WinFunc) ) continue;
            if( sAggInfo.aFunc[i].pFunc->funcFlags&SQLITE_FUNC_NEEDCOLL ) break;
          }
          if( i==sAggInfo.nFunc ){
            regAcc = ++pParse->nMem;
            sqlite3VdbeAddOp2(v, OP_Integer, 0, regAcc);
          }
        }

        /* This case runs if the aggregate has no GROUP BY clause.  The
        ** processing is much simpler since there is only a single row
        ** of output.
        */
        assert( p->pGroupBy==0 );
        resetAccumulator(pParse, &sAggInfo);

        /* If this query is a candidate for the min/max optimization, then
        ** minMaxFlag will have been previously set to either
        ** WHERE_ORDERBY_MIN or WHERE_ORDERBY_MAX and pMinMaxOrderBy will
        ** be an appropriate ORDER BY expression for the optimization.
        */
        assert( minMaxFlag==WHERE_ORDERBY_NORMAL || pMinMaxOrderBy!=0 );
        assert( pMinMaxOrderBy==0 || pMinMaxOrderBy->nExpr==1 );

        SELECTTRACE(1,pParse,p,("WhereBegin\n"));
        pWInfo = sqlite3WhereBegin(pParse, pTabList, pWhere, pMinMaxOrderBy,
                                   0, minMaxFlag, 0);
        if( pWInfo==0 ){
          goto select_end;
        }
        updateAccumulator(pParse, regAcc, &sAggInfo);
        if( regAcc ) sqlite3VdbeAddOp2(v, OP_Integer, 1, regAcc);
        if( sqlite3WhereIsOrdered(pWInfo)>0 ){
          sqlite3VdbeGoto(v, sqlite3WhereBreakLabel(pWInfo));
          VdbeComment((v, "%s() by index",
                (minMaxFlag==WHERE_ORDERBY_MIN?"min":"max")));
        }
        sqlite3WhereEnd(pWInfo);
        finalizeAggFunctions(pParse, &sAggInfo);
      }

      sSort.pOrderBy = 0;
      sqlite3ExprIfFalse(pParse, pHaving, addrEnd, SQLITE_JUMPIFNULL);
      selectInnerLoop(pParse, p, -1, 0, 0, 
                      pDest, addrEnd, addrEnd);
    }
    sqlite3VdbeResolveLabel(v, addrEnd);
    
  } /* endif aggregate query */

  if( sDistinct.eTnctType==WHERE_DISTINCT_UNORDERED ){
    explainTempTable(pParse, "DISTINCT");
  }

  /* If there is an ORDER BY clause, then we need to sort the results
  ** and send them to the callback one by one.
  */
  if( sSort.pOrderBy ){
    explainTempTable(pParse,
                     sSort.nOBSat>0 ? "RIGHT PART OF ORDER BY":"ORDER BY");
    assert( p->pEList==pEList );
    generateSortTail(pParse, p, &sSort, pEList->nExpr, pDest);
  }

  /* Jump here to skip this query
  */
  sqlite3VdbeResolveLabel(v, iEnd);

  /* The SELECT has been coded. If there is an error in the Parse structure,
  ** set the return code to 1. Otherwise 0. */
  rc = (pParse->nErr>0);

  /* Control jumps to here if an error is encountered above, or upon
  ** successful coding of the SELECT.
  */
select_end:
  sqlite3ExprListDelete(db, pMinMaxOrderBy);
  sqlite3DbFree(db, sAggInfo.aCol);
  sqlite3DbFree(db, sAggInfo.aFunc);
#if SELECTTRACE_ENABLED
  SELECTTRACE(0x1,pParse,p,("end processing\n"));
  if( (sqlite3SelectTrace & 0x2000)!=0 && ExplainQueryPlanParent(pParse)==0 ){
    sqlite3TreeViewSelect(0, p, 0);
  }
#endif
  ExplainQueryPlanPop(pParse);
  return rc;
}