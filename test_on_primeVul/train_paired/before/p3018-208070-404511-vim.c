nfa_regmatch(
    nfa_regprog_T	*prog,
    nfa_state_T		*start,
    regsubs_T		*submatch,
    regsubs_T		*m)
{
    int		result = FALSE;
    size_t	size = 0;
    int		flag = 0;
    int		go_to_nextline = FALSE;
    nfa_thread_T *t;
    nfa_list_T	list[2];
    int		listidx;
    nfa_list_T	*thislist;
    nfa_list_T	*nextlist;
    int		*listids = NULL;
    int		listids_len = 0;
    nfa_state_T *add_state;
    int		add_here;
    int		add_count;
    int		add_off = 0;
    int		toplevel = start->c == NFA_MOPEN;
    regsubs_T	*r;
#ifdef NFA_REGEXP_DEBUG_LOG
    FILE	*debug;
#endif

    // Some patterns may take a long time to match, especially when using
    // recursive_regmatch(). Allow interrupting them with CTRL-C.
    fast_breakcheck();
    if (got_int)
	return FALSE;
#ifdef FEAT_RELTIME
    if (nfa_did_time_out())
	return FALSE;
#endif

#ifdef NFA_REGEXP_DEBUG_LOG
    debug = fopen(NFA_REGEXP_DEBUG_LOG, "a");
    if (debug == NULL)
    {
	semsg("(NFA) COULD NOT OPEN %s!", NFA_REGEXP_DEBUG_LOG);
	return FALSE;
    }
#endif
    nfa_match = FALSE;

    // Allocate memory for the lists of nodes.
    size = (prog->nstate + 1) * sizeof(nfa_thread_T);

    list[0].t = alloc(size);
    list[0].len = prog->nstate + 1;
    list[1].t = alloc(size);
    list[1].len = prog->nstate + 1;
    if (list[0].t == NULL || list[1].t == NULL)
	goto theend;

#ifdef ENABLE_LOG
    log_fd = fopen(NFA_REGEXP_RUN_LOG, "a");
    if (log_fd != NULL)
    {
	fprintf(log_fd, "**********************************\n");
	nfa_set_code(start->c);
	fprintf(log_fd, " RUNNING nfa_regmatch() starting with state %d, code %s\n",
	abs(start->id), code);
	fprintf(log_fd, "**********************************\n");
    }
    else
    {
	emsg(_(e_log_open_failed));
	log_fd = stderr;
    }
#endif

    thislist = &list[0];
    thislist->n = 0;
    thislist->has_pim = FALSE;
    nextlist = &list[1];
    nextlist->n = 0;
    nextlist->has_pim = FALSE;
#ifdef ENABLE_LOG
    fprintf(log_fd, "(---) STARTSTATE first\n");
#endif
    thislist->id = rex.nfa_listid + 1;

    // Inline optimized code for addstate(thislist, start, m, 0) if we know
    // it's the first MOPEN.
    if (toplevel)
    {
	if (REG_MULTI)
	{
	    m->norm.list.multi[0].start_lnum = rex.lnum;
	    m->norm.list.multi[0].start_col = (colnr_T)(rex.input - rex.line);
	}
	else
	    m->norm.list.line[0].start = rex.input;
	m->norm.in_use = 1;
	r = addstate(thislist, start->out, m, NULL, 0);
    }
    else
	r = addstate(thislist, start, m, NULL, 0);
    if (r == NULL)
    {
	nfa_match = NFA_TOO_EXPENSIVE;
	goto theend;
    }

#define	ADD_STATE_IF_MATCH(state)			\
    if (result) {					\
	add_state = state->out;				\
	add_off = clen;					\
    }

    /*
     * Run for each character.
     */
    for (;;)
    {
	int	curc;
	int	clen;

	if (has_mbyte)
	{
	    curc = (*mb_ptr2char)(rex.input);
	    clen = (*mb_ptr2len)(rex.input);
	}
	else
	{
	    curc = *rex.input;
	    clen = 1;
	}
	if (curc == NUL)
	{
	    clen = 0;
	    go_to_nextline = FALSE;
	}

	// swap lists
	thislist = &list[flag];
	nextlist = &list[flag ^= 1];
	nextlist->n = 0;	    // clear nextlist
	nextlist->has_pim = FALSE;
	++rex.nfa_listid;
	if (prog->re_engine == AUTOMATIC_ENGINE
		&& (rex.nfa_listid >= NFA_MAX_STATES
# ifdef FEAT_EVAL
		    || nfa_fail_for_testing
# endif
		    ))
	{
	    // too many states, retry with old engine
	    nfa_match = NFA_TOO_EXPENSIVE;
	    goto theend;
	}

	thislist->id = rex.nfa_listid;
	nextlist->id = rex.nfa_listid + 1;

#ifdef ENABLE_LOG
	fprintf(log_fd, "------------------------------------------\n");
	fprintf(log_fd, ">>> Reginput is \"%s\"\n", rex.input);
	fprintf(log_fd, ">>> Advanced one character... Current char is %c (code %d) \n", curc, (int)curc);
	fprintf(log_fd, ">>> Thislist has %d states available: ", thislist->n);
	{
	    int i;

	    for (i = 0; i < thislist->n; i++)
		fprintf(log_fd, "%d  ", abs(thislist->t[i].state->id));
	}
	fprintf(log_fd, "\n");
#endif

#ifdef NFA_REGEXP_DEBUG_LOG
	fprintf(debug, "\n-------------------\n");
#endif
	/*
	 * If the state lists are empty we can stop.
	 */
	if (thislist->n == 0)
	    break;

	// compute nextlist
	for (listidx = 0; listidx < thislist->n; ++listidx)
	{
	    // If the list gets very long there probably is something wrong.
	    // At least allow interrupting with CTRL-C.
	    fast_breakcheck();
	    if (got_int)
		break;
#ifdef FEAT_RELTIME
	    if (nfa_time_limit != NULL && ++nfa_time_count == 20)
	    {
		nfa_time_count = 0;
		if (nfa_did_time_out())
		    break;
	    }
#endif
	    t = &thislist->t[listidx];

#ifdef NFA_REGEXP_DEBUG_LOG
	    nfa_set_code(t->state->c);
	    fprintf(debug, "%s, ", code);
#endif
#ifdef ENABLE_LOG
	    {
		int col;

		if (t->subs.norm.in_use <= 0)
		    col = -1;
		else if (REG_MULTI)
		    col = t->subs.norm.list.multi[0].start_col;
		else
		    col = (int)(t->subs.norm.list.line[0].start - rex.line);
		nfa_set_code(t->state->c);
		fprintf(log_fd, "(%d) char %d %s (start col %d)%s... \n",
			abs(t->state->id), (int)t->state->c, code, col,
			pim_info(&t->pim));
	    }
#endif

	    /*
	     * Handle the possible codes of the current state.
	     * The most important is NFA_MATCH.
	     */
	    add_state = NULL;
	    add_here = FALSE;
	    add_count = 0;
	    switch (t->state->c)
	    {
	    case NFA_MATCH:
	      {
		// If the match is not at the start of the line, ends before a
		// composing characters and rex.reg_icombine is not set, that
		// is not really a match.
		if (enc_utf8 && !rex.reg_icombine
			     && rex.input != rex.line && utf_iscomposing(curc))
		    break;

		nfa_match = TRUE;
		copy_sub(&submatch->norm, &t->subs.norm);
#ifdef FEAT_SYN_HL
		if (rex.nfa_has_zsubexpr)
		    copy_sub(&submatch->synt, &t->subs.synt);
#endif
#ifdef ENABLE_LOG
		log_subsexpr(&t->subs);
#endif
		// Found the left-most longest match, do not look at any other
		// states at this position.  When the list of states is going
		// to be empty quit without advancing, so that "rex.input" is
		// correct.
		if (nextlist->n == 0)
		    clen = 0;
		goto nextchar;
	      }

	    case NFA_END_INVISIBLE:
	    case NFA_END_INVISIBLE_NEG:
	    case NFA_END_PATTERN:
		/*
		 * This is only encountered after a NFA_START_INVISIBLE or
		 * NFA_START_INVISIBLE_BEFORE node.
		 * They surround a zero-width group, used with "\@=", "\&",
		 * "\@!", "\@<=" and "\@<!".
		 * If we got here, it means that the current "invisible" group
		 * finished successfully, so return control to the parent
		 * nfa_regmatch().  For a look-behind match only when it ends
		 * in the position in "nfa_endp".
		 * Submatches are stored in *m, and used in the parent call.
		 */
#ifdef ENABLE_LOG
		if (nfa_endp != NULL)
		{
		    if (REG_MULTI)
			fprintf(log_fd, "Current lnum: %d, endp lnum: %d; current col: %d, endp col: %d\n",
				(int)rex.lnum,
				(int)nfa_endp->se_u.pos.lnum,
				(int)(rex.input - rex.line),
				nfa_endp->se_u.pos.col);
		    else
			fprintf(log_fd, "Current col: %d, endp col: %d\n",
				(int)(rex.input - rex.line),
				(int)(nfa_endp->se_u.ptr - rex.input));
		}
#endif
		// If "nfa_endp" is set it's only a match if it ends at
		// "nfa_endp"
		if (nfa_endp != NULL && (REG_MULTI
			? (rex.lnum != nfa_endp->se_u.pos.lnum
			    || (int)(rex.input - rex.line)
						!= nfa_endp->se_u.pos.col)
			: rex.input != nfa_endp->se_u.ptr))
		    break;

		// do not set submatches for \@!
		if (t->state->c != NFA_END_INVISIBLE_NEG)
		{
		    copy_sub(&m->norm, &t->subs.norm);
#ifdef FEAT_SYN_HL
		    if (rex.nfa_has_zsubexpr)
			copy_sub(&m->synt, &t->subs.synt);
#endif
		}
#ifdef ENABLE_LOG
		fprintf(log_fd, "Match found:\n");
		log_subsexpr(m);
#endif
		nfa_match = TRUE;
		// See comment above at "goto nextchar".
		if (nextlist->n == 0)
		    clen = 0;
		goto nextchar;

	    case NFA_START_INVISIBLE:
	    case NFA_START_INVISIBLE_FIRST:
	    case NFA_START_INVISIBLE_NEG:
	    case NFA_START_INVISIBLE_NEG_FIRST:
	    case NFA_START_INVISIBLE_BEFORE:
	    case NFA_START_INVISIBLE_BEFORE_FIRST:
	    case NFA_START_INVISIBLE_BEFORE_NEG:
	    case NFA_START_INVISIBLE_BEFORE_NEG_FIRST:
		{
#ifdef ENABLE_LOG
		    fprintf(log_fd, "Failure chance invisible: %d, what follows: %d\n",
			    failure_chance(t->state->out, 0),
			    failure_chance(t->state->out1->out, 0));
#endif
		    // Do it directly if there already is a PIM or when
		    // nfa_postprocess() detected it will work better.
		    if (t->pim.result != NFA_PIM_UNUSED
			 || t->state->c == NFA_START_INVISIBLE_FIRST
			 || t->state->c == NFA_START_INVISIBLE_NEG_FIRST
			 || t->state->c == NFA_START_INVISIBLE_BEFORE_FIRST
			 || t->state->c == NFA_START_INVISIBLE_BEFORE_NEG_FIRST)
		    {
			int in_use = m->norm.in_use;

			// Copy submatch info for the recursive call, opposite
			// of what happens on success below.
			copy_sub_off(&m->norm, &t->subs.norm);
#ifdef FEAT_SYN_HL
			if (rex.nfa_has_zsubexpr)
			    copy_sub_off(&m->synt, &t->subs.synt);
#endif

			/*
			 * First try matching the invisible match, then what
			 * follows.
			 */
			result = recursive_regmatch(t->state, NULL, prog,
					  submatch, m, &listids, &listids_len);
			if (result == NFA_TOO_EXPENSIVE)
			{
			    nfa_match = result;
			    goto theend;
			}

			// for \@! and \@<! it is a match when the result is
			// FALSE
			if (result != (t->state->c == NFA_START_INVISIBLE_NEG
			       || t->state->c == NFA_START_INVISIBLE_NEG_FIRST
			       || t->state->c
					   == NFA_START_INVISIBLE_BEFORE_NEG
			       || t->state->c
				     == NFA_START_INVISIBLE_BEFORE_NEG_FIRST))
			{
			    // Copy submatch info from the recursive call
			    copy_sub_off(&t->subs.norm, &m->norm);
#ifdef FEAT_SYN_HL
			    if (rex.nfa_has_zsubexpr)
				copy_sub_off(&t->subs.synt, &m->synt);
#endif
			    // If the pattern has \ze and it matched in the
			    // sub pattern, use it.
			    copy_ze_off(&t->subs.norm, &m->norm);

			    // t->state->out1 is the corresponding
			    // END_INVISIBLE node; Add its out to the current
			    // list (zero-width match).
			    add_here = TRUE;
			    add_state = t->state->out1->out;
			}
			m->norm.in_use = in_use;
		    }
		    else
		    {
			nfa_pim_T pim;

			/*
			 * First try matching what follows.  Only if a match
			 * is found verify the invisible match matches.  Add a
			 * nfa_pim_T to the following states, it contains info
			 * about the invisible match.
			 */
			pim.state = t->state;
			pim.result = NFA_PIM_TODO;
			pim.subs.norm.in_use = 0;
#ifdef FEAT_SYN_HL
			pim.subs.synt.in_use = 0;
#endif
			if (REG_MULTI)
			{
			    pim.end.pos.col = (int)(rex.input - rex.line);
			    pim.end.pos.lnum = rex.lnum;
			}
			else
			    pim.end.ptr = rex.input;

			// t->state->out1 is the corresponding END_INVISIBLE
			// node; Add its out to the current list (zero-width
			// match).
			if (addstate_here(thislist, t->state->out1->out,
					     &t->subs, &pim, &listidx) == NULL)
			{
			    nfa_match = NFA_TOO_EXPENSIVE;
			    goto theend;
			}
		    }
		}
		break;

	    case NFA_START_PATTERN:
	      {
		nfa_state_T *skip = NULL;
#ifdef ENABLE_LOG
		int	    skip_lid = 0;
#endif

		// There is no point in trying to match the pattern if the
		// output state is not going to be added to the list.
		if (state_in_list(nextlist, t->state->out1->out, &t->subs))
		{
		    skip = t->state->out1->out;
#ifdef ENABLE_LOG
		    skip_lid = nextlist->id;
#endif
		}
		else if (state_in_list(nextlist,
					  t->state->out1->out->out, &t->subs))
		{
		    skip = t->state->out1->out->out;
#ifdef ENABLE_LOG
		    skip_lid = nextlist->id;
#endif
		}
		else if (state_in_list(thislist,
					  t->state->out1->out->out, &t->subs))
		{
		    skip = t->state->out1->out->out;
#ifdef ENABLE_LOG
		    skip_lid = thislist->id;
#endif
		}
		if (skip != NULL)
		{
#ifdef ENABLE_LOG
		    nfa_set_code(skip->c);
		    fprintf(log_fd, "> Not trying to match pattern, output state %d is already in list %d. char %d: %s\n",
			    abs(skip->id), skip_lid, skip->c, code);
#endif
		    break;
		}
		// Copy submatch info to the recursive call, opposite of what
		// happens afterwards.
		copy_sub_off(&m->norm, &t->subs.norm);
#ifdef FEAT_SYN_HL
		if (rex.nfa_has_zsubexpr)
		    copy_sub_off(&m->synt, &t->subs.synt);
#endif

		// First try matching the pattern.
		result = recursive_regmatch(t->state, NULL, prog,
					  submatch, m, &listids, &listids_len);
		if (result == NFA_TOO_EXPENSIVE)
		{
		    nfa_match = result;
		    goto theend;
		}
		if (result)
		{
		    int bytelen;

#ifdef ENABLE_LOG
		    fprintf(log_fd, "NFA_START_PATTERN matches:\n");
		    log_subsexpr(m);
#endif
		    // Copy submatch info from the recursive call
		    copy_sub_off(&t->subs.norm, &m->norm);
#ifdef FEAT_SYN_HL
		    if (rex.nfa_has_zsubexpr)
			copy_sub_off(&t->subs.synt, &m->synt);
#endif
		    // Now we need to skip over the matched text and then
		    // continue with what follows.
		    if (REG_MULTI)
			// TODO: multi-line match
			bytelen = m->norm.list.multi[0].end_col
						  - (int)(rex.input - rex.line);
		    else
			bytelen = (int)(m->norm.list.line[0].end - rex.input);

#ifdef ENABLE_LOG
		    fprintf(log_fd, "NFA_START_PATTERN length: %d\n", bytelen);
#endif
		    if (bytelen == 0)
		    {
			// empty match, output of corresponding
			// NFA_END_PATTERN/NFA_SKIP to be used at current
			// position
			add_here = TRUE;
			add_state = t->state->out1->out->out;
		    }
		    else if (bytelen <= clen)
		    {
			// match current character, output of corresponding
			// NFA_END_PATTERN to be used at next position.
			add_state = t->state->out1->out->out;
			add_off = clen;
		    }
		    else
		    {
			// skip over the matched characters, set character
			// count in NFA_SKIP
			add_state = t->state->out1->out;
			add_off = bytelen;
			add_count = bytelen - clen;
		    }
		}
		break;
	      }

	    case NFA_BOL:
		if (rex.input == rex.line)
		{
		    add_here = TRUE;
		    add_state = t->state->out;
		}
		break;

	    case NFA_EOL:
		if (curc == NUL)
		{
		    add_here = TRUE;
		    add_state = t->state->out;
		}
		break;

	    case NFA_BOW:
		result = TRUE;

		if (curc == NUL)
		    result = FALSE;
		else if (has_mbyte)
		{
		    int this_class;

		    // Get class of current and previous char (if it exists).
		    this_class = mb_get_class_buf(rex.input, rex.reg_buf);
		    if (this_class <= 1)
			result = FALSE;
		    else if (reg_prev_class() == this_class)
			result = FALSE;
		}
		else if (!vim_iswordc_buf(curc, rex.reg_buf)
			   || (rex.input > rex.line
				&& vim_iswordc_buf(rex.input[-1], rex.reg_buf)))
		    result = FALSE;
		if (result)
		{
		    add_here = TRUE;
		    add_state = t->state->out;
		}
		break;

	    case NFA_EOW:
		result = TRUE;
		if (rex.input == rex.line)
		    result = FALSE;
		else if (has_mbyte)
		{
		    int this_class, prev_class;

		    // Get class of current and previous char (if it exists).
		    this_class = mb_get_class_buf(rex.input, rex.reg_buf);
		    prev_class = reg_prev_class();
		    if (this_class == prev_class
					|| prev_class == 0 || prev_class == 1)
			result = FALSE;
		}
		else if (!vim_iswordc_buf(rex.input[-1], rex.reg_buf)
			|| (rex.input[0] != NUL
					&& vim_iswordc_buf(curc, rex.reg_buf)))
		    result = FALSE;
		if (result)
		{
		    add_here = TRUE;
		    add_state = t->state->out;
		}
		break;

	    case NFA_BOF:
		if (rex.lnum == 0 && rex.input == rex.line
				     && (!REG_MULTI || rex.reg_firstlnum == 1))
		{
		    add_here = TRUE;
		    add_state = t->state->out;
		}
		break;

	    case NFA_EOF:
		if (rex.lnum == rex.reg_maxline && curc == NUL)
		{
		    add_here = TRUE;
		    add_state = t->state->out;
		}
		break;

	    case NFA_COMPOSING:
	    {
		int	    mc = curc;
		int	    len = 0;
		nfa_state_T *end;
		nfa_state_T *sta;
		int	    cchars[MAX_MCO];
		int	    ccount = 0;
		int	    j;

		sta = t->state->out;
		len = 0;
		if (utf_iscomposing(sta->c))
		{
		    // Only match composing character(s), ignore base
		    // character.  Used for ".{composing}" and "{composing}"
		    // (no preceding character).
		    len += mb_char2len(mc);
		}
		if (rex.reg_icombine && len == 0)
		{
		    // If \Z was present, then ignore composing characters.
		    // When ignoring the base character this always matches.
		    if (sta->c != curc)
			result = FAIL;
		    else
			result = OK;
		    while (sta->c != NFA_END_COMPOSING)
			sta = sta->out;
		}

		// Check base character matches first, unless ignored.
		else if (len > 0 || mc == sta->c)
		{
		    if (len == 0)
		    {
			len += mb_char2len(mc);
			sta = sta->out;
		    }

		    // We don't care about the order of composing characters.
		    // Get them into cchars[] first.
		    while (len < clen)
		    {
			mc = mb_ptr2char(rex.input + len);
			cchars[ccount++] = mc;
			len += mb_char2len(mc);
			if (ccount == MAX_MCO)
			    break;
		    }

		    // Check that each composing char in the pattern matches a
		    // composing char in the text.  We do not check if all
		    // composing chars are matched.
		    result = OK;
		    while (sta->c != NFA_END_COMPOSING)
		    {
			for (j = 0; j < ccount; ++j)
			    if (cchars[j] == sta->c)
				break;
			if (j == ccount)
			{
			    result = FAIL;
			    break;
			}
			sta = sta->out;
		    }
		}
		else
		    result = FAIL;

		end = t->state->out1;	    // NFA_END_COMPOSING
		ADD_STATE_IF_MATCH(end);
		break;
	    }

	    case NFA_NEWL:
		if (curc == NUL && !rex.reg_line_lbr && REG_MULTI
						 && rex.lnum <= rex.reg_maxline)
		{
		    go_to_nextline = TRUE;
		    // Pass -1 for the offset, which means taking the position
		    // at the start of the next line.
		    add_state = t->state->out;
		    add_off = -1;
		}
		else if (curc == '\n' && rex.reg_line_lbr)
		{
		    // match \n as if it is an ordinary character
		    add_state = t->state->out;
		    add_off = 1;
		}
		break;

	    case NFA_START_COLL:
	    case NFA_START_NEG_COLL:
	      {
		// What follows is a list of characters, until NFA_END_COLL.
		// One of them must match or none of them must match.
		nfa_state_T	*state;
		int		result_if_matched;
		int		c1, c2;

		// Never match EOL. If it's part of the collection it is added
		// as a separate state with an OR.
		if (curc == NUL)
		    break;

		state = t->state->out;
		result_if_matched = (t->state->c == NFA_START_COLL);
		for (;;)
		{
		    if (state->c == NFA_END_COLL)
		    {
			result = !result_if_matched;
			break;
		    }
		    if (state->c == NFA_RANGE_MIN)
		    {
			c1 = state->val;
			state = state->out; // advance to NFA_RANGE_MAX
			c2 = state->val;
#ifdef ENABLE_LOG
			fprintf(log_fd, "NFA_RANGE_MIN curc=%d c1=%d c2=%d\n",
				curc, c1, c2);
#endif
			if (curc >= c1 && curc <= c2)
			{
			    result = result_if_matched;
			    break;
			}
			if (rex.reg_ic)
			{
			    int curc_low = MB_CASEFOLD(curc);
			    int done = FALSE;

			    for ( ; c1 <= c2; ++c1)
				if (MB_CASEFOLD(c1) == curc_low)
				{
				    result = result_if_matched;
				    done = TRUE;
				    break;
				}
			    if (done)
				break;
			}
		    }
		    else if (state->c < 0 ? check_char_class(state->c, curc)
			       : (curc == state->c
				   || (rex.reg_ic && MB_CASEFOLD(curc)
						    == MB_CASEFOLD(state->c))))
		    {
			result = result_if_matched;
			break;
		    }
		    state = state->out;
		}
		if (result)
		{
		    // next state is in out of the NFA_END_COLL, out1 of
		    // START points to the END state
		    add_state = t->state->out1->out;
		    add_off = clen;
		}
		break;
	      }

	    case NFA_ANY:
		// Any char except '\0', (end of input) does not match.
		if (curc > 0)
		{
		    add_state = t->state->out;
		    add_off = clen;
		}
		break;

	    case NFA_ANY_COMPOSING:
		// On a composing character skip over it.  Otherwise do
		// nothing.  Always matches.
		if (enc_utf8 && utf_iscomposing(curc))
		{
		    add_off = clen;
		}
		else
		{
		    add_here = TRUE;
		    add_off = 0;
		}
		add_state = t->state->out;
		break;

	    /*
	     * Character classes like \a for alpha, \d for digit etc.
	     */
	    case NFA_IDENT:	//  \i
		result = vim_isIDc(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_SIDENT:	//  \I
		result = !VIM_ISDIGIT(curc) && vim_isIDc(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_KWORD:	//  \k
		result = vim_iswordp_buf(rex.input, rex.reg_buf);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_SKWORD:	//  \K
		result = !VIM_ISDIGIT(curc)
				     && vim_iswordp_buf(rex.input, rex.reg_buf);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_FNAME:	//  \f
		result = vim_isfilec(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_SFNAME:	//  \F
		result = !VIM_ISDIGIT(curc) && vim_isfilec(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_PRINT:	//  \p
		result = vim_isprintc(PTR2CHAR(rex.input));
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_SPRINT:	//  \P
		result = !VIM_ISDIGIT(curc) && vim_isprintc(PTR2CHAR(rex.input));
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_WHITE:	//  \s
		result = VIM_ISWHITE(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_NWHITE:	//  \S
		result = curc != NUL && !VIM_ISWHITE(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_DIGIT:	//  \d
		result = ri_digit(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_NDIGIT:	//  \D
		result = curc != NUL && !ri_digit(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_HEX:	//  \x
		result = ri_hex(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_NHEX:	//  \X
		result = curc != NUL && !ri_hex(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_OCTAL:	//  \o
		result = ri_octal(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_NOCTAL:	//  \O
		result = curc != NUL && !ri_octal(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_WORD:	//  \w
		result = ri_word(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_NWORD:	//  \W
		result = curc != NUL && !ri_word(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_HEAD:	//  \h
		result = ri_head(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_NHEAD:	//  \H
		result = curc != NUL && !ri_head(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_ALPHA:	//  \a
		result = ri_alpha(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_NALPHA:	//  \A
		result = curc != NUL && !ri_alpha(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_LOWER:	//  \l
		result = ri_lower(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_NLOWER:	//  \L
		result = curc != NUL && !ri_lower(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_UPPER:	//  \u
		result = ri_upper(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_NUPPER:	// \U
		result = curc != NUL && !ri_upper(curc);
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_LOWER_IC:	// [a-z]
		result = ri_lower(curc) || (rex.reg_ic && ri_upper(curc));
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_NLOWER_IC:	// [^a-z]
		result = curc != NUL
			&& !(ri_lower(curc) || (rex.reg_ic && ri_upper(curc)));
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_UPPER_IC:	// [A-Z]
		result = ri_upper(curc) || (rex.reg_ic && ri_lower(curc));
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_NUPPER_IC:	// ^[A-Z]
		result = curc != NUL
			&& !(ri_upper(curc) || (rex.reg_ic && ri_lower(curc)));
		ADD_STATE_IF_MATCH(t->state);
		break;

	    case NFA_BACKREF1:
	    case NFA_BACKREF2:
	    case NFA_BACKREF3:
	    case NFA_BACKREF4:
	    case NFA_BACKREF5:
	    case NFA_BACKREF6:
	    case NFA_BACKREF7:
	    case NFA_BACKREF8:
	    case NFA_BACKREF9:
#ifdef FEAT_SYN_HL
	    case NFA_ZREF1:
	    case NFA_ZREF2:
	    case NFA_ZREF3:
	    case NFA_ZREF4:
	    case NFA_ZREF5:
	    case NFA_ZREF6:
	    case NFA_ZREF7:
	    case NFA_ZREF8:
	    case NFA_ZREF9:
#endif
		// \1 .. \9  \z1 .. \z9
	      {
		int subidx;
		int bytelen;

		if (t->state->c <= NFA_BACKREF9)
		{
		    subidx = t->state->c - NFA_BACKREF1 + 1;
		    result = match_backref(&t->subs.norm, subidx, &bytelen);
		}
#ifdef FEAT_SYN_HL
		else
		{
		    subidx = t->state->c - NFA_ZREF1 + 1;
		    result = match_zref(subidx, &bytelen);
		}
#endif

		if (result)
		{
		    if (bytelen == 0)
		    {
			// empty match always works, output of NFA_SKIP to be
			// used next
			add_here = TRUE;
			add_state = t->state->out->out;
		    }
		    else if (bytelen <= clen)
		    {
			// match current character, jump ahead to out of
			// NFA_SKIP
			add_state = t->state->out->out;
			add_off = clen;
		    }
		    else
		    {
			// skip over the matched characters, set character
			// count in NFA_SKIP
			add_state = t->state->out;
			add_off = bytelen;
			add_count = bytelen - clen;
		    }
		}
		break;
	      }
	    case NFA_SKIP:
	      // character of previous matching \1 .. \9  or \@>
	      if (t->count - clen <= 0)
	      {
		  // end of match, go to what follows
		  add_state = t->state->out;
		  add_off = clen;
	      }
	      else
	      {
		  // add state again with decremented count
		  add_state = t->state;
		  add_off = 0;
		  add_count = t->count - clen;
	      }
	      break;

	    case NFA_LNUM:
	    case NFA_LNUM_GT:
	    case NFA_LNUM_LT:
		result = (REG_MULTI &&
			nfa_re_num_cmp(t->state->val, t->state->c - NFA_LNUM,
			    (long_u)(rex.lnum + rex.reg_firstlnum)));
		if (result)
		{
		    add_here = TRUE;
		    add_state = t->state->out;
		}
		break;

	    case NFA_COL:
	    case NFA_COL_GT:
	    case NFA_COL_LT:
		result = nfa_re_num_cmp(t->state->val, t->state->c - NFA_COL,
			(long_u)(rex.input - rex.line) + 1);
		if (result)
		{
		    add_here = TRUE;
		    add_state = t->state->out;
		}
		break;

	    case NFA_VCOL:
	    case NFA_VCOL_GT:
	    case NFA_VCOL_LT:
		{
		    int     op = t->state->c - NFA_VCOL;
		    colnr_T col = (colnr_T)(rex.input - rex.line);
		    win_T   *wp = rex.reg_win == NULL ? curwin : rex.reg_win;

		    // Bail out quickly when there can't be a match, avoid the
		    // overhead of win_linetabsize() on long lines.
		    if (op != 1 && col > t->state->val
			    * (has_mbyte ? MB_MAXBYTES : 1))
			break;
		    result = FALSE;
		    if (op == 1 && col - 1 > t->state->val && col > 100)
		    {
			int ts = wp->w_buffer->b_p_ts;

			// Guess that a character won't use more columns than
			// 'tabstop', with a minimum of 4.
			if (ts < 4)
			    ts = 4;
			result = col > t->state->val * ts;
		    }
		    if (!result)
			result = nfa_re_num_cmp(t->state->val, op,
				(long_u)win_linetabsize(wp, rex.line, col) + 1);
		    if (result)
		    {
			add_here = TRUE;
			add_state = t->state->out;
		    }
		}
		break;

	    case NFA_MARK:
	    case NFA_MARK_GT:
	    case NFA_MARK_LT:
	      {
		pos_T	*pos = getmark_buf(rex.reg_buf, t->state->val, FALSE);

		// Compare the mark position to the match position, if the mark
		// exists and mark is set in reg_buf.
		if (pos != NULL && pos->lnum > 0)
		{
		    colnr_T pos_col = pos->lnum == rex.lnum + rex.reg_firstlnum
							  && pos->col == MAXCOL
				      ? (colnr_T)STRLEN(reg_getline(
						pos->lnum - rex.reg_firstlnum))
				      : pos->col;

		    result = (pos->lnum == rex.lnum + rex.reg_firstlnum
				? (pos_col == (colnr_T)(rex.input - rex.line)
				    ? t->state->c == NFA_MARK
				    : (pos_col < (colnr_T)(rex.input - rex.line)
					? t->state->c == NFA_MARK_GT
					: t->state->c == NFA_MARK_LT))
				: (pos->lnum < rex.lnum + rex.reg_firstlnum
				    ? t->state->c == NFA_MARK_GT
				    : t->state->c == NFA_MARK_LT));
		    if (result)
		    {
			add_here = TRUE;
			add_state = t->state->out;
		    }
		}
		break;
	      }

	    case NFA_CURSOR:
		result = (rex.reg_win != NULL
			&& (rex.lnum + rex.reg_firstlnum
						 == rex.reg_win->w_cursor.lnum)
			&& ((colnr_T)(rex.input - rex.line)
						== rex.reg_win->w_cursor.col));
		if (result)
		{
		    add_here = TRUE;
		    add_state = t->state->out;
		}
		break;

	    case NFA_VISUAL:
		result = reg_match_visual();
		if (result)
		{
		    add_here = TRUE;
		    add_state = t->state->out;
		}
		break;

	    case NFA_MOPEN1:
	    case NFA_MOPEN2:
	    case NFA_MOPEN3:
	    case NFA_MOPEN4:
	    case NFA_MOPEN5:
	    case NFA_MOPEN6:
	    case NFA_MOPEN7:
	    case NFA_MOPEN8:
	    case NFA_MOPEN9:
#ifdef FEAT_SYN_HL
	    case NFA_ZOPEN:
	    case NFA_ZOPEN1:
	    case NFA_ZOPEN2:
	    case NFA_ZOPEN3:
	    case NFA_ZOPEN4:
	    case NFA_ZOPEN5:
	    case NFA_ZOPEN6:
	    case NFA_ZOPEN7:
	    case NFA_ZOPEN8:
	    case NFA_ZOPEN9:
#endif
	    case NFA_NOPEN:
	    case NFA_ZSTART:
		// These states are only added to be able to bail out when
		// they are added again, nothing is to be done.
		break;

	    default:	// regular character
	      {
		int c = t->state->c;

#ifdef DEBUG
		if (c < 0)
		    siemsg("INTERNAL: Negative state char: %ld", c);
#endif
		result = (c == curc);

		if (!result && rex.reg_ic)
		    result = MB_CASEFOLD(c) == MB_CASEFOLD(curc);
		// If rex.reg_icombine is not set only skip over the character
		// itself.  When it is set skip over composing characters.
		if (result && enc_utf8 && !rex.reg_icombine)
		    clen = utf_ptr2len(rex.input);
		ADD_STATE_IF_MATCH(t->state);
		break;
	      }

	    } // switch (t->state->c)

	    if (add_state != NULL)
	    {
		nfa_pim_T *pim;
		nfa_pim_T pim_copy;

		if (t->pim.result == NFA_PIM_UNUSED)
		    pim = NULL;
		else
		    pim = &t->pim;

		// Handle the postponed invisible match if the match might end
		// without advancing and before the end of the line.
		if (pim != NULL && (clen == 0 || match_follows(add_state, 0)))
		{
		    if (pim->result == NFA_PIM_TODO)
		    {
#ifdef ENABLE_LOG
			fprintf(log_fd, "\n");
			fprintf(log_fd, "==================================\n");
			fprintf(log_fd, "Postponed recursive nfa_regmatch()\n");
			fprintf(log_fd, "\n");
#endif
			result = recursive_regmatch(pim->state, pim,
				    prog, submatch, m, &listids, &listids_len);
			pim->result = result ? NFA_PIM_MATCH : NFA_PIM_NOMATCH;
			// for \@! and \@<! it is a match when the result is
			// FALSE
			if (result != (pim->state->c == NFA_START_INVISIBLE_NEG
			     || pim->state->c == NFA_START_INVISIBLE_NEG_FIRST
			     || pim->state->c
					   == NFA_START_INVISIBLE_BEFORE_NEG
			     || pim->state->c
				     == NFA_START_INVISIBLE_BEFORE_NEG_FIRST))
			{
			    // Copy submatch info from the recursive call
			    copy_sub_off(&pim->subs.norm, &m->norm);
#ifdef FEAT_SYN_HL
			    if (rex.nfa_has_zsubexpr)
				copy_sub_off(&pim->subs.synt, &m->synt);
#endif
			}
		    }
		    else
		    {
			result = (pim->result == NFA_PIM_MATCH);
#ifdef ENABLE_LOG
			fprintf(log_fd, "\n");
			fprintf(log_fd, "Using previous recursive nfa_regmatch() result, result == %d\n", pim->result);
			fprintf(log_fd, "MATCH = %s\n", result == TRUE ? "OK" : "FALSE");
			fprintf(log_fd, "\n");
#endif
		    }

		    // for \@! and \@<! it is a match when result is FALSE
		    if (result != (pim->state->c == NFA_START_INVISIBLE_NEG
			     || pim->state->c == NFA_START_INVISIBLE_NEG_FIRST
			     || pim->state->c
					   == NFA_START_INVISIBLE_BEFORE_NEG
			     || pim->state->c
				     == NFA_START_INVISIBLE_BEFORE_NEG_FIRST))
		    {
			// Copy submatch info from the recursive call
			copy_sub_off(&t->subs.norm, &pim->subs.norm);
#ifdef FEAT_SYN_HL
			if (rex.nfa_has_zsubexpr)
			    copy_sub_off(&t->subs.synt, &pim->subs.synt);
#endif
		    }
		    else
			// look-behind match failed, don't add the state
			continue;

		    // Postponed invisible match was handled, don't add it to
		    // following states.
		    pim = NULL;
		}

		// If "pim" points into l->t it will become invalid when
		// adding the state causes the list to be reallocated.  Make a
		// local copy to avoid that.
		if (pim == &t->pim)
		{
		    copy_pim(&pim_copy, pim);
		    pim = &pim_copy;
		}

		if (add_here)
		    r = addstate_here(thislist, add_state, &t->subs,
								pim, &listidx);
		else
		{
		    r = addstate(nextlist, add_state, &t->subs, pim, add_off);
		    if (add_count > 0)
			nextlist->t[nextlist->n - 1].count = add_count;
		}
		if (r == NULL)
		{
		    nfa_match = NFA_TOO_EXPENSIVE;
		    goto theend;
		}
	    }

	} // for (thislist = thislist; thislist->state; thislist++)

	// Look for the start of a match in the current position by adding the
	// start state to the list of states.
	// The first found match is the leftmost one, thus the order of states
	// matters!
	// Do not add the start state in recursive calls of nfa_regmatch(),
	// because recursive calls should only start in the first position.
	// Unless "nfa_endp" is not NULL, then we match the end position.
	// Also don't start a match past the first line.
	if (nfa_match == FALSE
		&& ((toplevel
			&& rex.lnum == 0
			&& clen != 0
			&& (rex.reg_maxcol == 0
			    || (colnr_T)(rex.input - rex.line) < rex.reg_maxcol))
		    || (nfa_endp != NULL
			&& (REG_MULTI
			    ? (rex.lnum < nfa_endp->se_u.pos.lnum
			       || (rex.lnum == nfa_endp->se_u.pos.lnum
				   && (int)(rex.input - rex.line)
						    < nfa_endp->se_u.pos.col))
			    : rex.input < nfa_endp->se_u.ptr))))
	{
#ifdef ENABLE_LOG
	    fprintf(log_fd, "(---) STARTSTATE\n");
#endif
	    // Inline optimized code for addstate() if we know the state is
	    // the first MOPEN.
	    if (toplevel)
	    {
		int add = TRUE;
		int c;

		if (prog->regstart != NUL && clen != 0)
		{
		    if (nextlist->n == 0)
		    {
			colnr_T col = (colnr_T)(rex.input - rex.line) + clen;

			// Nextlist is empty, we can skip ahead to the
			// character that must appear at the start.
			if (skip_to_start(prog->regstart, &col) == FAIL)
			    break;
#ifdef ENABLE_LOG
			fprintf(log_fd, "  Skipping ahead %d bytes to regstart\n",
				col - ((colnr_T)(rex.input - rex.line) + clen));
#endif
			rex.input = rex.line + col - clen;
		    }
		    else
		    {
			// Checking if the required start character matches is
			// cheaper than adding a state that won't match.
			c = PTR2CHAR(rex.input + clen);
			if (c != prog->regstart && (!rex.reg_ic
			     || MB_CASEFOLD(c) != MB_CASEFOLD(prog->regstart)))
			{
#ifdef ENABLE_LOG
			    fprintf(log_fd, "  Skipping start state, regstart does not match\n");
#endif
			    add = FALSE;
			}
		    }
		}

		if (add)
		{
		    if (REG_MULTI)
			m->norm.list.multi[0].start_col =
					 (colnr_T)(rex.input - rex.line) + clen;
		    else
			m->norm.list.line[0].start = rex.input + clen;
		    if (addstate(nextlist, start->out, m, NULL, clen) == NULL)
		    {
			nfa_match = NFA_TOO_EXPENSIVE;
			goto theend;
		    }
		}
	    }
	    else
	    {
		if (addstate(nextlist, start, m, NULL, clen) == NULL)
		{
		    nfa_match = NFA_TOO_EXPENSIVE;
		    goto theend;
		}
	    }
	}

#ifdef ENABLE_LOG
	fprintf(log_fd, ">>> Thislist had %d states available: ", thislist->n);
	{
	    int i;

	    for (i = 0; i < thislist->n; i++)
		fprintf(log_fd, "%d  ", abs(thislist->t[i].state->id));
	}
	fprintf(log_fd, "\n");
#endif

nextchar:
	// Advance to the next character, or advance to the next line, or
	// finish.
	if (clen != 0)
	    rex.input += clen;
	else if (go_to_nextline || (nfa_endp != NULL && REG_MULTI
					&& rex.lnum < nfa_endp->se_u.pos.lnum))
	    reg_nextline();
	else
	    break;

	// Allow interrupting with CTRL-C.
	line_breakcheck();
	if (got_int)
	    break;
#ifdef FEAT_RELTIME
	// Check for timeout once in a twenty times to avoid overhead.
	if (nfa_time_limit != NULL && ++nfa_time_count == 20)
	{
	    nfa_time_count = 0;
	    if (nfa_did_time_out())
		break;
	}
#endif
    }

#ifdef ENABLE_LOG
    if (log_fd != stderr)
	fclose(log_fd);
    log_fd = NULL;
#endif

theend:
    // Free memory
    vim_free(list[0].t);
    vim_free(list[1].t);
    vim_free(listids);
#undef ADD_STATE_IF_MATCH
#ifdef NFA_REGEXP_DEBUG_LOG
    fclose(debug);
#endif

    return nfa_match;
}