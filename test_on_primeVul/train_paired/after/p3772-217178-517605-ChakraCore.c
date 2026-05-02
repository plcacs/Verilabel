ParseNodePtr Parser::ParseTerm(BOOL fAllowCall,
    LPCOLESTR pNameHint,
    uint32 *pHintLength,
    uint32 *pShortNameOffset,
    _Inout_opt_ IdentToken* pToken /*= nullptr*/,
    bool fUnaryOrParen /*= false*/,
    _Out_opt_ BOOL* pfCanAssign /*= nullptr*/,
    _Inout_opt_ BOOL* pfLikelyPattern /*= nullptr*/,
    _Out_opt_ bool* pfIsDotOrIndex /*= nullptr*/,
    _Inout_opt_ charcount_t *plastRParen /*= nullptr*/)
{
    ParseNodePtr pnode = nullptr;
    PidRefStack *savedTopAsyncRef = nullptr;
    charcount_t ichMin = 0;
    size_t iecpMin = 0;
    size_t iuMin;
    IdentToken term;
    BOOL fInNew = FALSE;
    BOOL fCanAssign = TRUE;
    bool isAsyncExpr = false;
    bool isLambdaExpr = false;
    Assert(pToken == nullptr || pToken->tk == tkNone); // Must be empty initially

    if (this->IsBackgroundParser())
    {
        PROBE_STACK_NO_DISPOSE(m_scriptContext, Js::Constants::MinStackParseOneTerm);
    }
    else
    {
        PROBE_STACK(m_scriptContext, Js::Constants::MinStackParseOneTerm);
    }

    switch (m_token.tk)
    {
    case tkID:
    {
        PidRefStack *ref = nullptr;
        IdentPtr pid = m_token.GetIdentifier(m_phtbl);
        charcount_t ichLim = m_pscan->IchLimTok();
        size_t iecpLim = m_pscan->IecpLimTok();
        ichMin = m_pscan->IchMinTok();
        iecpMin  = m_pscan->IecpMinTok();

        if (pid == wellKnownPropertyPids.async &&
            m_scriptContext->GetConfig()->IsES7AsyncAndAwaitEnabled())
        {
            isAsyncExpr = true;
        }

        bool previousAwaitIsKeyword = m_pscan->SetAwaitIsKeyword(isAsyncExpr);
        m_pscan->Scan();
        m_pscan->SetAwaitIsKeyword(previousAwaitIsKeyword);

        // We search for an Async expression (a function declaration or an async lambda expression)
        if (isAsyncExpr && !m_pscan->FHadNewLine())
        {
            if (m_token.tk == tkFUNCTION)
            {
                goto LFunction;
            }
            else if (m_token.tk == tkID || m_token.tk == tkAWAIT)
            {
                isLambdaExpr = true;
                goto LFunction;
            }
            else if (m_token.tk == tkLParen)
            {
                // This is potentially an async arrow function. Save the state of the async references
                // in case it needs to be restored. (Note that the case of a single parameter with no ()'s
                // is detected upstream and need not be handled here.)
                savedTopAsyncRef = pid->GetTopRef();
            }
        }

        // Don't push a reference if this is a single lambda parameter, because we'll reparse with
        // a correct function ID.
        if (m_token.tk != tkDArrow)
        {
            ref = this->PushPidRef(pid);
        }

        if (buildAST)
        {
            pnode = CreateNameNode(pid);
            pnode->ichMin = ichMin;
            pnode->ichLim = ichLim;
            pnode->sxPid.SetSymRef(ref);
        }
        else
        {
            // Remember the identifier start and end in case it turns out to be a statement label.
            term.tk = tkID;
            term.pid = pid; // Record the identifier for detection of eval
            term.ichMin = static_cast<charcount_t>(iecpMin);
            term.ichLim = static_cast<charcount_t>(iecpLim);
        }
        CheckArgumentsUse(pid, GetCurrentFunctionNode());
        break;
    }

    case tkTHIS:
        if (buildAST)
        {
            pnode = CreateNodeWithScanner<knopThis>();
        }
        fCanAssign = FALSE;
        m_pscan->Scan();
        break;

    case tkLParen:
    {
        ichMin = m_pscan->IchMinTok();
        iuMin = m_pscan->IecpMinTok();
        m_pscan->Scan();
        if (m_token.tk == tkRParen)
        {
            // Empty parens can only be legal as an empty parameter list to a lambda declaration.
            // We're in a lambda if the next token is =>.
            fAllowCall = FALSE;
            m_pscan->Scan();

            // If the token after the right paren is not => or if there was a newline between () and => this is a syntax error
            if (!m_doingFastScan && (m_token.tk != tkDArrow || m_pscan->FHadNewLine()))
            {
                Error(ERRsyntax);
            }

            if (buildAST)
            {
                pnode = CreateNodeWithScanner<knopEmpty>();
            }
            break;
        }

        // Advance the block ID here in case this parenthetical expression turns out to be a lambda parameter list.
        // That way the pid ref stacks will be created in their correct final form, and we can simply fix
        // up function ID's.
        uint saveNextBlockId = m_nextBlockId;
        uint saveCurrBlockId = GetCurrentBlock()->sxBlock.blockId;
        GetCurrentBlock()->sxBlock.blockId = m_nextBlockId++;

        this->m_parenDepth++;
        pnode = ParseExpr<buildAST>(koplNo, &fCanAssign, TRUE, FALSE, nullptr, nullptr /*nameLength*/, nullptr  /*pShortNameOffset*/, &term, true, nullptr, plastRParen);
        this->m_parenDepth--;

        if (buildAST && plastRParen)
        {
            *plastRParen = m_pscan->IchLimTok();
        }

        ChkCurTok(tkRParen, ERRnoRparen);

        GetCurrentBlock()->sxBlock.blockId = saveCurrBlockId;
        if (m_token.tk == tkDArrow)
        {
            // We're going to rewind and reinterpret the expression as a parameter list.
            // Put back the original next-block-ID so the existing pid ref stacks will be correct.
            m_nextBlockId = saveNextBlockId;
        }

        // Emit a deferred ... error if one was parsed.
        if (m_deferEllipsisError && m_token.tk != tkDArrow)
        {
            m_pscan->SeekTo(m_EllipsisErrLoc);
            Error(ERRInvalidSpreadUse);
        }
        else
        {
            m_deferEllipsisError = false;
        }
        break;
    }

    case tkIntCon:
        if (IsStrictMode() && m_pscan->IsOctOrLeadingZeroOnLastTKNumber())
        {
            Error(ERRES5NoOctal);
        }

        if (buildAST)
        {
            pnode = CreateIntNodeWithScanner(m_token.GetLong());
        }
        fCanAssign = FALSE;
        m_pscan->Scan();
        break;

    case tkFltCon:
        if (IsStrictMode() && m_pscan->IsOctOrLeadingZeroOnLastTKNumber())
        {
            Error(ERRES5NoOctal);
        }

        if (buildAST)
        {
            pnode = CreateNodeWithScanner<knopFlt>();
            pnode->sxFlt.dbl = m_token.GetDouble();
            pnode->sxFlt.maybeInt = m_token.GetDoubleMayBeInt();
        }
        fCanAssign = FALSE;
        m_pscan->Scan();
        break;

    case tkStrCon:
        if (IsStrictMode() && m_pscan->IsOctOrLeadingZeroOnLastTKNumber())
        {
            Error(ERRES5NoOctal);
        }

        if (buildAST)
        {
            pnode = CreateStrNodeWithScanner(m_token.GetStr());
        }
        else
        {
            // Subtract the string literal length from the total char count for the purpose
            // of deciding whether to defer parsing and byte code generation.
            this->ReduceDeferredScriptLength(m_pscan->IchLimTok() - m_pscan->IchMinTok());
        }
        fCanAssign = FALSE;
        m_pscan->Scan();
        break;

    case tkTRUE:
        if (buildAST)
        {
            pnode = CreateNodeWithScanner<knopTrue>();
        }
        fCanAssign = FALSE;
        m_pscan->Scan();
        break;

    case tkFALSE:
        if (buildAST)
        {
            pnode = CreateNodeWithScanner<knopFalse>();
        }
        fCanAssign = FALSE;
        m_pscan->Scan();
        break;

    case tkNULL:
        if (buildAST)
        {
            pnode = CreateNodeWithScanner<knopNull>();
        }
        fCanAssign = FALSE;
        m_pscan->Scan();
        break;

    case tkDiv:
    case tkAsgDiv:
        pnode = ParseRegExp<buildAST>();
        fCanAssign = FALSE;
        m_pscan->Scan();
        break;

    case tkNEW:
    {
        ichMin = m_pscan->IchMinTok();
        m_pscan->Scan();

        if (m_token.tk == tkDot && m_scriptContext->GetConfig()->IsES6ClassAndExtendsEnabled())
        {
            pnode = ParseMetaProperty<buildAST>(tkNEW, ichMin, &fCanAssign);

            m_pscan->Scan();
        }
        else
        {
            ParseNodePtr pnodeExpr = ParseTerm<buildAST>(FALSE, pNameHint, pHintLength, pShortNameOffset);
            if (buildAST)
            {
                pnode = CreateCallNode(knopNew, pnodeExpr, nullptr);
                pnode->ichMin = ichMin;
            }
            fInNew = TRUE;
            fCanAssign = FALSE;
        }
        break;
    }

    case tkLBrack:
    {
        ichMin = m_pscan->IchMinTok();
        m_pscan->Scan();
        pnode = ParseArrayLiteral<buildAST>();
        if (buildAST)
        {
            pnode->ichMin = ichMin;
            pnode->ichLim = m_pscan->IchLimTok();
        }

        if (this->m_arrayDepth == 0)
        {
            Assert(m_pscan->IchLimTok() - ichMin > m_funcInArray);
            this->ReduceDeferredScriptLength(m_pscan->IchLimTok() - ichMin - this->m_funcInArray);
            this->m_funcInArray = 0;
            this->m_funcInArrayDepth = 0;
        }
        ChkCurTok(tkRBrack, ERRnoRbrack);
        if (!IsES6DestructuringEnabled())
        {
            fCanAssign = FALSE;
        }
        else if (pfLikelyPattern != nullptr && !IsPostFixOperators())
        {
            *pfLikelyPattern = TRUE;
        }
        break;
    }

    case tkLCurly:
    {
        ichMin = m_pscan->IchMinTok();
        m_pscan->ScanForcingPid();
        ParseNodePtr pnodeMemberList = ParseMemberList<buildAST>(pNameHint, pHintLength);
        if (buildAST)
        {
            pnode = CreateUniNode(knopObject, pnodeMemberList);
            pnode->ichMin = ichMin;
            pnode->ichLim = m_pscan->IchLimTok();
        }
        ChkCurTok(tkRCurly, ERRnoRcurly);
        if (!IsES6DestructuringEnabled())
        {
            fCanAssign = FALSE;
        }
        else if (pfLikelyPattern != nullptr && !IsPostFixOperators())
        {
            *pfLikelyPattern = TRUE;
        }
        break;
    }

    case tkFUNCTION:
    {
LFunction :
        if (m_grfscr & fscrDeferredFncExpression)
        {
            // The top-level deferred function body was defined by a function expression whose parsing was deferred. We are now
            // parsing it, so unset the flag so that any nested functions are parsed normally. This flag is only applicable the
            // first time we see it.
            //
            // Normally, deferred functions will be parsed in ParseStatement upon encountering the 'function' token. The first
            // token of the source code of the function may not a 'function' token though, so we still need to reset this flag
            // for the first function we parse. This can happen in compat modes, for instance, for a function expression enclosed
            // in parentheses, where the legacy behavior was to include the parentheses in the function's source code.
            m_grfscr &= ~fscrDeferredFncExpression;
        }
        ushort flags = fFncNoFlgs;
        if (isLambdaExpr)
        {
            flags |= fFncLambda;
        }
        if (isAsyncExpr)
        {
            flags |= fFncAsync;
        }
        pnode = ParseFncDecl<buildAST>(flags, pNameHint, false, true, fUnaryOrParen);
        if (isAsyncExpr)
        {
            pnode->sxFnc.cbMin = iecpMin;
            pnode->ichMin = ichMin;
        }
        fCanAssign = FALSE;
        break;
    }

    case tkCLASS:
        if (m_scriptContext->GetConfig()->IsES6ClassAndExtendsEnabled())
        {
            pnode = ParseClassDecl<buildAST>(FALSE, pNameHint, pHintLength, pShortNameOffset);
        }
        else
        {
            goto LUnknown;
        }
        fCanAssign = FALSE;
        break;

    case tkStrTmplBasic:
    case tkStrTmplBegin:
        pnode = ParseStringTemplateDecl<buildAST>(nullptr);
        fCanAssign = FALSE;
        break;

    case tkSUPER:
        if (m_scriptContext->GetConfig()->IsES6ClassAndExtendsEnabled())
        {
            pnode = ParseSuper<buildAST>(pnode, !!fAllowCall);
        }
        else
        {
            goto LUnknown;
        }
        break;

    case tkCASE:
    {
        if (!m_doingFastScan)
        {
            goto LUnknown;
        }
        ParseNodePtr pnodeUnused;
        pnode = ParseCase<buildAST>(&pnodeUnused);
        break;
    }

    case tkELSE:
        if (!m_doingFastScan)
        {
            goto LUnknown;
        }
        m_pscan->Scan();
        ParseStatement<buildAST>();
        break;

    default:
    LUnknown :
        Error(ERRsyntax);
        break;
    }

    pnode = ParsePostfixOperators<buildAST>(pnode, fAllowCall, fInNew, isAsyncExpr, &fCanAssign, &term, pfIsDotOrIndex);

    if (savedTopAsyncRef != nullptr &&
        this->m_token.tk == tkDArrow)
    {
        // This is an async arrow function; we're going to back up and reparse it.
        // Make sure we don't leave behind a bogus reference to the 'async' identifier.
        for (IdentPtr pid = wellKnownPropertyPids.async; pid->GetTopRef() != savedTopAsyncRef;)
        {
            Assert(pid->GetTopRef() != nullptr);
            pid->RemovePrevPidRef(nullptr);
        }
    }

    // Pass back identifier if requested
    if (pToken && term.tk == tkID)
    {
        *pToken = term;
    }

    if (pfCanAssign)
    {
        *pfCanAssign = fCanAssign;
    }

    return pnode;
}