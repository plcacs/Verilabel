nv_brackets(cmdarg_T *cap)
{
    pos_T	prev_pos;
    pos_T	*pos = NULL;	    // init for GCC
    pos_T	old_pos;	    // cursor position before command
    int		flag;
    long	n;

    cap->oap->motion_type = MCHAR;
    cap->oap->inclusive = FALSE;
    old_pos = curwin->w_cursor;
    curwin->w_cursor.coladd = 0;    // TODO: don't do this for an error.

#ifdef FEAT_SEARCHPATH
    // "[f" or "]f" : Edit file under the cursor (same as "gf")
    if (cap->nchar == 'f')
	nv_gotofile(cap);
    else
#endif

#ifdef FEAT_FIND_ID
    // Find the occurrence(s) of the identifier or define under cursor
    // in current and included files or jump to the first occurrence.
    //
    //			search	     list	    jump
    //		      fwd   bwd    fwd	 bwd	 fwd	bwd
    // identifier     "]i"  "[i"   "]I"  "[I"	"]^I"  "[^I"
    // define	      "]d"  "[d"   "]D"  "[D"	"]^D"  "[^D"
    if (vim_strchr((char_u *)"iI\011dD\004", cap->nchar) != NULL)
    {
	char_u	*ptr;
	int	len;

	if ((len = find_ident_under_cursor(&ptr, FIND_IDENT)) == 0)
	    clearop(cap->oap);
	else
	{
	    find_pattern_in_path(ptr, 0, len, TRUE,
		cap->count0 == 0 ? !isupper(cap->nchar) : FALSE,
		((cap->nchar & 0xf) == ('d' & 0xf)) ?  FIND_DEFINE : FIND_ANY,
		cap->count1,
		isupper(cap->nchar) ? ACTION_SHOW_ALL :
			    islower(cap->nchar) ? ACTION_SHOW : ACTION_GOTO,
		cap->cmdchar == ']' ? curwin->w_cursor.lnum + 1 : (linenr_T)1,
		(linenr_T)MAXLNUM);
	    curwin->w_set_curswant = TRUE;
	}
    }
    else
#endif

    // "[{", "[(", "]}" or "])": go to Nth unclosed '{', '(', '}' or ')'
    // "[#", "]#": go to start/end of Nth innermost #if..#endif construct.
    // "[/", "[*", "]/", "]*": go to Nth comment start/end.
    // "[m" or "]m" search for prev/next start of (Java) method.
    // "[M" or "]M" search for prev/next end of (Java) method.
    if (  (cap->cmdchar == '['
		&& vim_strchr((char_u *)"{(*/#mM", cap->nchar) != NULL)
	    || (cap->cmdchar == ']'
		&& vim_strchr((char_u *)"})*/#mM", cap->nchar) != NULL))
	nv_bracket_block(cap, &old_pos);

    // "[[", "[]", "]]" and "][": move to start or end of function
    else if (cap->nchar == '[' || cap->nchar == ']')
    {
	if (cap->nchar == cap->cmdchar)		    // "]]" or "[["
	    flag = '{';
	else
	    flag = '}';		    // "][" or "[]"

	curwin->w_set_curswant = TRUE;
	// Imitate strange Vi behaviour: When using "]]" with an operator
	// we also stop at '}'.
	if (!findpar(&cap->oap->inclusive, cap->arg, cap->count1, flag,
	      (cap->oap->op_type != OP_NOP
				      && cap->arg == FORWARD && flag == '{')))
	    clearopbeep(cap->oap);
	else
	{
	    if (cap->oap->op_type == OP_NOP)
		beginline(BL_WHITE | BL_FIX);
#ifdef FEAT_FOLDING
	    if ((fdo_flags & FDO_BLOCK) && KeyTyped && cap->oap->op_type == OP_NOP)
		foldOpenCursor();
#endif
	}
    }

    // "[p", "[P", "]P" and "]p": put with indent adjustment
    else if (cap->nchar == 'p' || cap->nchar == 'P')
    {
	nv_put_opt(cap, TRUE);
    }

    // "['", "[`", "]'" and "]`": jump to next mark
    else if (cap->nchar == '\'' || cap->nchar == '`')
    {
	pos = &curwin->w_cursor;
	for (n = cap->count1; n > 0; --n)
	{
	    prev_pos = *pos;
	    pos = getnextmark(pos, cap->cmdchar == '[' ? BACKWARD : FORWARD,
							  cap->nchar == '\'');
	    if (pos == NULL)
		break;
	}
	if (pos == NULL)
	    pos = &prev_pos;
	nv_cursormark(cap, cap->nchar == '\'', pos);
    }

    // [ or ] followed by a middle mouse click: put selected text with
    // indent adjustment.  Any other button just does as usual.
    else if (cap->nchar >= K_RIGHTRELEASE && cap->nchar <= K_LEFTMOUSE)
    {
	(void)do_mouse(cap->oap, cap->nchar,
		       (cap->cmdchar == ']') ? FORWARD : BACKWARD,
		       cap->count1, PUT_FIXINDENT);
    }

#ifdef FEAT_FOLDING
    // "[z" and "]z": move to start or end of open fold.
    else if (cap->nchar == 'z')
    {
	if (foldMoveTo(FALSE, cap->cmdchar == ']' ? FORWARD : BACKWARD,
							 cap->count1) == FAIL)
	    clearopbeep(cap->oap);
    }
#endif

#ifdef FEAT_DIFF
    // "[c" and "]c": move to next or previous diff-change.
    else if (cap->nchar == 'c')
    {
	if (diff_move_to(cap->cmdchar == ']' ? FORWARD : BACKWARD,
							 cap->count1) == FAIL)
	    clearopbeep(cap->oap);
    }
#endif

#ifdef FEAT_SPELL
    // "[s", "[S", "]s" and "]S": move to next spell error.
    else if (cap->nchar == 's' || cap->nchar == 'S')
    {
	setpcmark();
	for (n = 0; n < cap->count1; ++n)
	    if (spell_move_to(curwin, cap->cmdchar == ']' ? FORWARD : BACKWARD,
			  cap->nchar == 's' ? TRUE : FALSE, FALSE, NULL) == 0)
	    {
		clearopbeep(cap->oap);
		break;
	    }
	    else
		curwin->w_set_curswant = TRUE;
# ifdef FEAT_FOLDING
	if (cap->oap->op_type == OP_NOP && (fdo_flags & FDO_SEARCH) && KeyTyped)
	    foldOpenCursor();
# endif
    }
#endif

    // Not a valid cap->nchar.
    else
	clearopbeep(cap->oap);
}