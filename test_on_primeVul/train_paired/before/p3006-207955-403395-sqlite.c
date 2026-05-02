static int lookupName(
  Parse *pParse,       /* The parsing context */
  const char *zDb,     /* Name of the database containing table, or NULL */
  const char *zTab,    /* Name of table containing column, or NULL */
  const char *zCol,    /* Name of the column. */
  NameContext *pNC,    /* The name context used to resolve the name */
  Expr *pExpr          /* Make this EXPR node point to the selected column */
){
  int i, j;                         /* Loop counters */
  int cnt = 0;                      /* Number of matching column names */
  int cntTab = 0;                   /* Number of matching table names */
  int nSubquery = 0;                /* How many levels of subquery */
  sqlite3 *db = pParse->db;         /* The database connection */
  struct SrcList_item *pItem;       /* Use for looping over pSrcList items */
  struct SrcList_item *pMatch = 0;  /* The matching pSrcList item */
  NameContext *pTopNC = pNC;        /* First namecontext in the list */
  Schema *pSchema = 0;              /* Schema of the expression */
  int eNewExprOp = TK_COLUMN;       /* New value for pExpr->op on success */
  Table *pTab = 0;                  /* Table hold the row */
  Column *pCol;                     /* A column of pTab */

  assert( pNC );     /* the name context cannot be NULL. */
  assert( zCol );    /* The Z in X.Y.Z cannot be NULL */
  assert( !ExprHasProperty(pExpr, EP_TokenOnly|EP_Reduced) );

  /* Initialize the node to no-match */
  pExpr->iTable = -1;
  ExprSetVVAProperty(pExpr, EP_NoReduce);

  /* Translate the schema name in zDb into a pointer to the corresponding
  ** schema.  If not found, pSchema will remain NULL and nothing will match
  ** resulting in an appropriate error message toward the end of this routine
  */
  if( zDb ){
    testcase( pNC->ncFlags & NC_PartIdx );
    testcase( pNC->ncFlags & NC_IsCheck );
    if( (pNC->ncFlags & (NC_PartIdx|NC_IsCheck))!=0 ){
      /* Silently ignore database qualifiers inside CHECK constraints and
      ** partial indices.  Do not raise errors because that might break
      ** legacy and because it does not hurt anything to just ignore the
      ** database name. */
      zDb = 0;
    }else{
      for(i=0; i<db->nDb; i++){
        assert( db->aDb[i].zDbSName );
        if( sqlite3StrICmp(db->aDb[i].zDbSName,zDb)==0 ){
          pSchema = db->aDb[i].pSchema;
          break;
        }
      }
    }
  }

  /* Start at the inner-most context and move outward until a match is found */
  assert( pNC && cnt==0 );
  do{
    ExprList *pEList;
    SrcList *pSrcList = pNC->pSrcList;

    if( pSrcList ){
      for(i=0, pItem=pSrcList->a; i<pSrcList->nSrc; i++, pItem++){
        pTab = pItem->pTab;
        assert( pTab!=0 && pTab->zName!=0 );
        assert( pTab->nCol>0 );
        if( pItem->pSelect && (pItem->pSelect->selFlags & SF_NestedFrom)!=0 ){
          int hit = 0;
          pEList = pItem->pSelect->pEList;
          for(j=0; j<pEList->nExpr; j++){
            if( sqlite3MatchSpanName(pEList->a[j].zSpan, zCol, zTab, zDb) ){
              cnt++;
              cntTab = 2;
              pMatch = pItem;
              pExpr->iColumn = j;
              hit = 1;
            }
          }
          if( hit || zTab==0 ) continue;
        }
        if( zDb && pTab->pSchema!=pSchema ){
          continue;
        }
        if( zTab ){
          const char *zTabName = pItem->zAlias ? pItem->zAlias : pTab->zName;
          assert( zTabName!=0 );
          if( sqlite3StrICmp(zTabName, zTab)!=0 ){
            continue;
          }
          if( IN_RENAME_OBJECT && pItem->zAlias ){
            sqlite3RenameTokenRemap(pParse, 0, (void*)&pExpr->y.pTab);
          }
        }
        if( 0==(cntTab++) ){
          pMatch = pItem;
        }
        for(j=0, pCol=pTab->aCol; j<pTab->nCol; j++, pCol++){
          if( sqlite3StrICmp(pCol->zName, zCol)==0 ){
            /* If there has been exactly one prior match and this match
            ** is for the right-hand table of a NATURAL JOIN or is in a 
            ** USING clause, then skip this match.
            */
            if( cnt==1 ){
              if( pItem->fg.jointype & JT_NATURAL ) continue;
              if( nameInUsingClause(pItem->pUsing, zCol) ) continue;
            }
            cnt++;
            pMatch = pItem;
            /* Substitute the rowid (column -1) for the INTEGER PRIMARY KEY */
            pExpr->iColumn = j==pTab->iPKey ? -1 : (i16)j;
            break;
          }
        }
      }
      if( pMatch ){
        pExpr->iTable = pMatch->iCursor;
        pExpr->y.pTab = pMatch->pTab;
        /* RIGHT JOIN not (yet) supported */
        assert( (pMatch->fg.jointype & JT_RIGHT)==0 );
        if( (pMatch->fg.jointype & JT_LEFT)!=0 ){
          ExprSetProperty(pExpr, EP_CanBeNull);
        }
        pSchema = pExpr->y.pTab->pSchema;
      }
    } /* if( pSrcList ) */

#if !defined(SQLITE_OMIT_TRIGGER) || !defined(SQLITE_OMIT_UPSERT)
    /* If we have not already resolved the name, then maybe 
    ** it is a new.* or old.* trigger argument reference.  Or
    ** maybe it is an excluded.* from an upsert.
    */
    if( zDb==0 && zTab!=0 && cntTab==0 ){
      pTab = 0;
#ifndef SQLITE_OMIT_TRIGGER
      if( pParse->pTriggerTab!=0 ){
        int op = pParse->eTriggerOp;
        assert( op==TK_DELETE || op==TK_UPDATE || op==TK_INSERT );
        if( op!=TK_DELETE && sqlite3StrICmp("new",zTab) == 0 ){
          pExpr->iTable = 1;
          pTab = pParse->pTriggerTab;
        }else if( op!=TK_INSERT && sqlite3StrICmp("old",zTab)==0 ){
          pExpr->iTable = 0;
          pTab = pParse->pTriggerTab;
        }
      }
#endif /* SQLITE_OMIT_TRIGGER */
#ifndef SQLITE_OMIT_UPSERT
      if( (pNC->ncFlags & NC_UUpsert)!=0 ){
        Upsert *pUpsert = pNC->uNC.pUpsert;
        if( pUpsert && sqlite3StrICmp("excluded",zTab)==0 ){
          pTab = pUpsert->pUpsertSrc->a[0].pTab;
          pExpr->iTable = 2;
        }
      }
#endif /* SQLITE_OMIT_UPSERT */

      if( pTab ){ 
        int iCol;
        pSchema = pTab->pSchema;
        cntTab++;
        for(iCol=0, pCol=pTab->aCol; iCol<pTab->nCol; iCol++, pCol++){
          if( sqlite3StrICmp(pCol->zName, zCol)==0 ){
            if( iCol==pTab->iPKey ){
              iCol = -1;
            }
            break;
          }
        }
        if( iCol>=pTab->nCol && sqlite3IsRowid(zCol) && VisibleRowid(pTab) ){
          /* IMP: R-51414-32910 */
          iCol = -1;
        }
        if( iCol<pTab->nCol ){
          cnt++;
#ifndef SQLITE_OMIT_UPSERT
          if( pExpr->iTable==2 ){
            testcase( iCol==(-1) );
            if( IN_RENAME_OBJECT ){
              pExpr->iColumn = iCol;
              pExpr->y.pTab = pTab;
              eNewExprOp = TK_COLUMN;
            }else{
              pExpr->iTable = pNC->uNC.pUpsert->regData + iCol;
              eNewExprOp = TK_REGISTER;
              ExprSetProperty(pExpr, EP_Alias);
            }
          }else
#endif /* SQLITE_OMIT_UPSERT */
          {
#ifndef SQLITE_OMIT_TRIGGER
            if( iCol<0 ){
              pExpr->affExpr = SQLITE_AFF_INTEGER;
            }else if( pExpr->iTable==0 ){
              testcase( iCol==31 );
              testcase( iCol==32 );
              pParse->oldmask |= (iCol>=32 ? 0xffffffff : (((u32)1)<<iCol));
            }else{
              testcase( iCol==31 );
              testcase( iCol==32 );
              pParse->newmask |= (iCol>=32 ? 0xffffffff : (((u32)1)<<iCol));
            }
            pExpr->y.pTab = pTab;
            pExpr->iColumn = (i16)iCol;
            eNewExprOp = TK_TRIGGER;
#endif /* SQLITE_OMIT_TRIGGER */
          }
        }
      }
    }
#endif /* !defined(SQLITE_OMIT_TRIGGER) || !defined(SQLITE_OMIT_UPSERT) */

    /*
    ** Perhaps the name is a reference to the ROWID
    */
    if( cnt==0
     && cntTab==1
     && pMatch
     && (pNC->ncFlags & (NC_IdxExpr|NC_GenCol))==0
     && sqlite3IsRowid(zCol)
     && VisibleRowid(pMatch->pTab)
    ){
      cnt = 1;
      pExpr->iColumn = -1;
      pExpr->affExpr = SQLITE_AFF_INTEGER;
    }

    /*
    ** If the input is of the form Z (not Y.Z or X.Y.Z) then the name Z
    ** might refer to an result-set alias.  This happens, for example, when
    ** we are resolving names in the WHERE clause of the following command:
    **
    **     SELECT a+b AS x FROM table WHERE x<10;
    **
    ** In cases like this, replace pExpr with a copy of the expression that
    ** forms the result set entry ("a+b" in the example) and return immediately.
    ** Note that the expression in the result set should have already been
    ** resolved by the time the WHERE clause is resolved.
    **
    ** The ability to use an output result-set column in the WHERE, GROUP BY,
    ** or HAVING clauses, or as part of a larger expression in the ORDER BY
    ** clause is not standard SQL.  This is a (goofy) SQLite extension, that
    ** is supported for backwards compatibility only. Hence, we issue a warning
    ** on sqlite3_log() whenever the capability is used.
    */
    if( (pNC->ncFlags & NC_UEList)!=0
     && cnt==0
     && zTab==0
    ){
      pEList = pNC->uNC.pEList;
      assert( pEList!=0 );
      for(j=0; j<pEList->nExpr; j++){
        char *zAs = pEList->a[j].zName;
        if( zAs!=0 && sqlite3StrICmp(zAs, zCol)==0 ){
          Expr *pOrig;
          assert( pExpr->pLeft==0 && pExpr->pRight==0 );
          assert( pExpr->x.pList==0 );
          assert( pExpr->x.pSelect==0 );
          pOrig = pEList->a[j].pExpr;
          if( (pNC->ncFlags&NC_AllowAgg)==0 && ExprHasProperty(pOrig, EP_Agg) ){
            sqlite3ErrorMsg(pParse, "misuse of aliased aggregate %s", zAs);
            return WRC_Abort;
          }
          if( (pNC->ncFlags&NC_AllowWin)==0 && ExprHasProperty(pOrig, EP_Win) ){
            sqlite3ErrorMsg(pParse, "misuse of aliased window function %s",zAs);
            return WRC_Abort;
          }
          if( sqlite3ExprVectorSize(pOrig)!=1 ){
            sqlite3ErrorMsg(pParse, "row value misused");
            return WRC_Abort;
          }
          resolveAlias(pParse, pEList, j, pExpr, "", nSubquery);
          cnt = 1;
          pMatch = 0;
          assert( zTab==0 && zDb==0 );
          if( IN_RENAME_OBJECT ){
            sqlite3RenameTokenRemap(pParse, 0, (void*)pExpr);
          }
          goto lookupname_end;
        }
      } 
    }

    /* Advance to the next name context.  The loop will exit when either
    ** we have a match (cnt>0) or when we run out of name contexts.
    */
    if( cnt ) break;
    pNC = pNC->pNext;
    nSubquery++;
  }while( pNC );


  /*
  ** If X and Y are NULL (in other words if only the column name Z is
  ** supplied) and the value of Z is enclosed in double-quotes, then
  ** Z is a string literal if it doesn't match any column names.  In that
  ** case, we need to return right away and not make any changes to
  ** pExpr.
  **
  ** Because no reference was made to outer contexts, the pNC->nRef
  ** fields are not changed in any context.
  */
  if( cnt==0 && zTab==0 ){
    assert( pExpr->op==TK_ID );
    if( ExprHasProperty(pExpr,EP_DblQuoted)
     && areDoubleQuotedStringsEnabled(db, pTopNC)
    ){
      /* If a double-quoted identifier does not match any known column name,
      ** then treat it as a string.
      **
      ** This hack was added in the early days of SQLite in a misguided attempt
      ** to be compatible with MySQL 3.x, which used double-quotes for strings.
      ** I now sorely regret putting in this hack. The effect of this hack is
      ** that misspelled identifier names are silently converted into strings
      ** rather than causing an error, to the frustration of countless
      ** programmers. To all those frustrated programmers, my apologies.
      **
      ** Someday, I hope to get rid of this hack. Unfortunately there is
      ** a huge amount of legacy SQL that uses it. So for now, we just
      ** issue a warning.
      */
      sqlite3_log(SQLITE_WARNING,
        "double-quoted string literal: \"%w\"", zCol);
#ifdef SQLITE_ENABLE_NORMALIZE
      sqlite3VdbeAddDblquoteStr(db, pParse->pVdbe, zCol);
#endif
      pExpr->op = TK_STRING;
      pExpr->y.pTab = 0;
      return WRC_Prune;
    }
    if( sqlite3ExprIdToTrueFalse(pExpr) ){
      return WRC_Prune;
    }
  }

  /*
  ** cnt==0 means there was not match.  cnt>1 means there were two or
  ** more matches.  Either way, we have an error.
  */
  if( cnt!=1 ){
    const char *zErr;
    zErr = cnt==0 ? "no such column" : "ambiguous column name";
    if( zDb ){
      sqlite3ErrorMsg(pParse, "%s: %s.%s.%s", zErr, zDb, zTab, zCol);
    }else if( zTab ){
      sqlite3ErrorMsg(pParse, "%s: %s.%s", zErr, zTab, zCol);
    }else{
      sqlite3ErrorMsg(pParse, "%s: %s", zErr, zCol);
    }
    pParse->checkSchema = 1;
    pTopNC->nErr++;
  }

  /* If a column from a table in pSrcList is referenced, then record
  ** this fact in the pSrcList.a[].colUsed bitmask.  Column 0 causes
  ** bit 0 to be set.  Column 1 sets bit 1.  And so forth.  If the
  ** column number is greater than the number of bits in the bitmask
  ** then set the high-order bit of the bitmask.
  */
  if( pExpr->iColumn>=0 && pMatch!=0 ){
    int n = pExpr->iColumn;
    testcase( n==BMS-1 );
    if( n>=BMS ){
      n = BMS-1;
    }
    assert( pMatch->iCursor==pExpr->iTable );
    pMatch->colUsed |= ((Bitmask)1)<<n;
  }

  /* Clean up and return
  */
  sqlite3ExprDelete(db, pExpr->pLeft);
  pExpr->pLeft = 0;
  sqlite3ExprDelete(db, pExpr->pRight);
  pExpr->pRight = 0;
  pExpr->op = eNewExprOp;
  ExprSetProperty(pExpr, EP_Leaf);
lookupname_end:
  if( cnt==1 ){
    assert( pNC!=0 );
    if( !ExprHasProperty(pExpr, EP_Alias) ){
      sqlite3AuthRead(pParse, pExpr, pSchema, pNC->pSrcList);
    }
    /* Increment the nRef value on all name contexts from TopNC up to
    ** the point where the name matched. */
    for(;;){
      assert( pTopNC!=0 );
      pTopNC->nRef++;
      if( pTopNC==pNC ) break;
      pTopNC = pTopNC->pNext;
    }
    return WRC_Prune;
  } else {
    return WRC_Abort;
  }
}