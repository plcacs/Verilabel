HTMLtagproc1(struct parsed_tag *tag, struct html_feed_environ *h_env)
{
    char *p, *q, *r;
    int i, w, x, y, z, count, width;
    struct readbuffer *obuf = h_env->obuf;
    struct environment *envs = h_env->envs;
    Str tmp;
    int hseq;
    int cmd;
#ifdef ID_EXT
    char *id = NULL;
#endif				/* ID_EXT */

    cmd = tag->tagid;

    if (obuf->flag & RB_PRE) {
	switch (cmd) {
	case HTML_NOBR:
	case HTML_N_NOBR:
	case HTML_PRE_INT:
	case HTML_N_PRE_INT:
	    return 1;
	}
    }

    switch (cmd) {
    case HTML_B:
	obuf->in_bold++;
	if (obuf->in_bold > 1)
	    return 1;
	return 0;
    case HTML_N_B:
	if (obuf->in_bold == 1 && close_effect0(obuf, HTML_B))
	    obuf->in_bold = 0;
	if (obuf->in_bold > 0) {
	    obuf->in_bold--;
	    if (obuf->in_bold == 0)
		return 0;
	}
	return 1;
    case HTML_I:
	obuf->in_italic++;
	if (obuf->in_italic > 1)
	    return 1;
	return 0;
    case HTML_N_I:
	if (obuf->in_italic == 1 && close_effect0(obuf, HTML_I))
	    obuf->in_italic = 0;
	if (obuf->in_italic > 0) {
	    obuf->in_italic--;
	    if (obuf->in_italic == 0)
		return 0;
	}
	return 1;
    case HTML_U:
	obuf->in_under++;
	if (obuf->in_under > 1)
	    return 1;
	return 0;
    case HTML_N_U:
	if (obuf->in_under == 1 && close_effect0(obuf, HTML_U))
	    obuf->in_under = 0;
	if (obuf->in_under > 0) {
	    obuf->in_under--;
	    if (obuf->in_under == 0)
		return 0;
	}
	return 1;
    case HTML_EM:
	HTMLlineproc1("<i>", h_env);
	return 1;
    case HTML_N_EM:
	HTMLlineproc1("</i>", h_env);
	return 1;
    case HTML_STRONG:
	HTMLlineproc1("<b>", h_env);
	return 1;
    case HTML_N_STRONG:
	HTMLlineproc1("</b>", h_env);
	return 1;
    case HTML_Q:
	HTMLlineproc1("`", h_env);
	return 1;
    case HTML_N_Q:
	HTMLlineproc1("'", h_env);
	return 1;
    case HTML_P:
    case HTML_N_P:
	CLOSE_A;
	if (!(obuf->flag & RB_IGNORE_P)) {
	    flushline(h_env, obuf, envs[h_env->envc].indent, 1, h_env->limit);
	    do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
			 h_env->limit);
	}
	obuf->flag |= RB_IGNORE_P;
	if (cmd == HTML_P) {
	    set_alignment(obuf, tag);
	    obuf->flag |= RB_P;
	}
	return 1;
    case HTML_BR:
	flushline(h_env, obuf, envs[h_env->envc].indent, 1, h_env->limit);
	h_env->blank_lines = 0;
	return 1;
    case HTML_H:
	if (!(obuf->flag & (RB_PREMODE | RB_IGNORE_P))) {
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	    do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
			 h_env->limit);
	}
	HTMLlineproc1("<b>", h_env);
	set_alignment(obuf, tag);
	return 1;
    case HTML_N_H:
	HTMLlineproc1("</b>", h_env);
	if (!(obuf->flag & RB_PREMODE)) {
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	}
	do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	RB_RESTORE_FLAG(obuf);
	close_anchor(h_env, obuf);
	obuf->flag |= RB_IGNORE_P;
	return 1;
    case HTML_UL:
    case HTML_OL:
    case HTML_BLQ:
	CLOSE_A;
	if (!(obuf->flag & RB_IGNORE_P)) {
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	    if (!(obuf->flag & RB_PREMODE) &&
		(h_env->envc == 0 || cmd == HTML_BLQ))
		do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
			     h_env->limit);
	}
	PUSH_ENV(cmd);
	if (cmd == HTML_UL || cmd == HTML_OL) {
	    if (parsedtag_get_value(tag, ATTR_START, &count)) {
		envs[h_env->envc].count = count - 1;
	    }
	}
	if (cmd == HTML_OL) {
	    envs[h_env->envc].type = '1';
	    if (parsedtag_get_value(tag, ATTR_TYPE, &p)) {
		envs[h_env->envc].type = (int)*p;
	    }
	}
	if (cmd == HTML_UL)
	    envs[h_env->envc].type = ul_type(tag, 0);
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	return 1;
    case HTML_N_UL:
    case HTML_N_OL:
    case HTML_N_DL:
    case HTML_N_BLQ:
	CLOSE_DT;
	CLOSE_A;
	if (h_env->envc > 0) {
	    flushline(h_env, obuf, envs[h_env->envc - 1].indent, 0,
		      h_env->limit);
	    POP_ENV;
	    if (!(obuf->flag & RB_PREMODE) &&
		(h_env->envc == 0 || cmd == HTML_N_DL || cmd == HTML_N_BLQ)) {
		do_blankline(h_env, obuf,
			     envs[h_env->envc].indent,
			     INDENT_INCR, h_env->limit);
		obuf->flag |= RB_IGNORE_P;
	    }
	}
	close_anchor(h_env, obuf);
	return 1;
    case HTML_DL:
	CLOSE_A;
	if (!(obuf->flag & RB_IGNORE_P)) {
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	    if (!(obuf->flag & RB_PREMODE))
		do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
			     h_env->limit);
	}
	PUSH_ENV(cmd);
	if (parsedtag_exists(tag, ATTR_COMPACT))
	    envs[h_env->envc].env = HTML_DL_COMPACT;
	obuf->flag |= RB_IGNORE_P;
	return 1;
    case HTML_LI:
	CLOSE_A;
	CLOSE_DT;
	if (h_env->envc > 0) {
	    Str num;
	    flushline(h_env, obuf,
		      envs[h_env->envc - 1].indent, 0, h_env->limit);
	    envs[h_env->envc].count++;
	    if (parsedtag_get_value(tag, ATTR_VALUE, &p)) {
		count = atoi(p);
		if (count > 0)
		    envs[h_env->envc].count = count;
		else
		    envs[h_env->envc].count = 0;
	    }
	    switch (envs[h_env->envc].env) {
	    case HTML_UL:
		envs[h_env->envc].type = ul_type(tag, envs[h_env->envc].type);
		for (i = 0; i < INDENT_INCR - 3; i++)
		    push_charp(obuf, 1, NBSP, PC_ASCII);
		tmp = Strnew();
		switch (envs[h_env->envc].type) {
		case 'd':
		    push_symbol(tmp, UL_SYMBOL_DISC, symbol_width, 1);
		    break;
		case 'c':
		    push_symbol(tmp, UL_SYMBOL_CIRCLE, symbol_width, 1);
		    break;
		case 's':
		    push_symbol(tmp, UL_SYMBOL_SQUARE, symbol_width, 1);
		    break;
		default:
		    push_symbol(tmp,
				UL_SYMBOL((h_env->envc_real -
					   1) % MAX_UL_LEVEL), symbol_width,
				1);
		    break;
		}
		if (symbol_width == 1)
		    push_charp(obuf, 1, NBSP, PC_ASCII);
		push_str(obuf, symbol_width, tmp, PC_ASCII);
		push_charp(obuf, 1, NBSP, PC_ASCII);
		set_space_to_prevchar(obuf->prevchar);
		break;
	    case HTML_OL:
		if (parsedtag_get_value(tag, ATTR_TYPE, &p))
		    envs[h_env->envc].type = (int)*p;
		switch ((envs[h_env->envc].count > 0)? envs[h_env->envc].type: '1') {
		case 'i':
		    num = romanNumeral(envs[h_env->envc].count);
		    break;
		case 'I':
		    num = romanNumeral(envs[h_env->envc].count);
		    Strupper(num);
		    break;
		case 'a':
		    num = romanAlphabet(envs[h_env->envc].count);
		    break;
		case 'A':
		    num = romanAlphabet(envs[h_env->envc].count);
		    Strupper(num);
		    break;
		default:
		    num = Sprintf("%d", envs[h_env->envc].count);
		    break;
		}
		if (INDENT_INCR >= 4)
		    Strcat_charp(num, ". ");
		else
		    Strcat_char(num, '.');
		push_spaces(obuf, 1, INDENT_INCR - num->length);
		push_str(obuf, num->length, num, PC_ASCII);
		if (INDENT_INCR >= 4)
		    set_space_to_prevchar(obuf->prevchar);
		break;
	    default:
		push_spaces(obuf, 1, INDENT_INCR);
		break;
	    }
	}
	else {
	    flushline(h_env, obuf, 0, 0, h_env->limit);
	}
	obuf->flag |= RB_IGNORE_P;
	return 1;
    case HTML_DT:
	CLOSE_A;
	if (h_env->envc == 0 ||
	    (h_env->envc_real < h_env->nenv &&
	     envs[h_env->envc].env != HTML_DL &&
	     envs[h_env->envc].env != HTML_DL_COMPACT)) {
	    PUSH_ENV(HTML_DL);
	}
	if (h_env->envc > 0) {
	    flushline(h_env, obuf,
		      envs[h_env->envc - 1].indent, 0, h_env->limit);
	}
	if (!(obuf->flag & RB_IN_DT)) {
	    HTMLlineproc1("<b>", h_env);
	    obuf->flag |= RB_IN_DT;
	}
	obuf->flag |= RB_IGNORE_P;
	return 1;
    case HTML_DD:
	CLOSE_A;
	CLOSE_DT;
	if (h_env->envc == 0 ||
	    (h_env->envc_real < h_env->nenv &&
	     envs[h_env->envc].env != HTML_DL &&
	     envs[h_env->envc].env != HTML_DL_COMPACT)) {
	    PUSH_ENV(HTML_DL);
	}
	if (envs[h_env->envc].env == HTML_DL_COMPACT) {
	    if (obuf->pos > envs[h_env->envc].indent)
		flushline(h_env, obuf, envs[h_env->envc].indent, 0,
			  h_env->limit);
	    else
		push_spaces(obuf, 1, envs[h_env->envc].indent - obuf->pos);
	}
	else
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	/* obuf->flag |= RB_IGNORE_P; */
	return 1;
    case HTML_TITLE:
	close_anchor(h_env, obuf);
	process_title(tag);
	obuf->flag |= RB_TITLE;
	obuf->end_tag = HTML_N_TITLE;
	return 1;
    case HTML_N_TITLE:
	if (!(obuf->flag & RB_TITLE))
	    return 1;
	obuf->flag &= ~RB_TITLE;
	obuf->end_tag = 0;
	tmp = process_n_title(tag);
	if (tmp)
	    HTMLlineproc1(tmp->ptr, h_env);
	return 1;
    case HTML_TITLE_ALT:
	if (parsedtag_get_value(tag, ATTR_TITLE, &p))
	    h_env->title = html_unquote(p);
	return 0;
    case HTML_FRAMESET:
	PUSH_ENV(cmd);
	push_charp(obuf, 9, "--FRAME--", PC_ASCII);
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	return 0;
    case HTML_N_FRAMESET:
	if (h_env->envc > 0) {
	    POP_ENV;
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	}
	return 0;
    case HTML_NOFRAMES:
	CLOSE_A;
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	obuf->flag |= (RB_NOFRAMES | RB_IGNORE_P);
	/* istr = str; */
	return 1;
    case HTML_N_NOFRAMES:
	CLOSE_A;
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	obuf->flag &= ~RB_NOFRAMES;
	return 1;
    case HTML_FRAME:
	q = r = NULL;
	parsedtag_get_value(tag, ATTR_SRC, &q);
	parsedtag_get_value(tag, ATTR_NAME, &r);
	if (q) {
	    q = html_quote(q);
	    push_tag(obuf, Sprintf("<a hseq=\"%d\" href=\"%s\">",
				   cur_hseq++, q)->ptr, HTML_A);
	    if (r)
		q = html_quote(r);
	    push_charp(obuf, get_strwidth(q), q, PC_ASCII);
	    push_tag(obuf, "</a>", HTML_N_A);
	}
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	return 0;
    case HTML_HR:
	close_anchor(h_env, obuf);
	tmp = process_hr(tag, h_env->limit, envs[h_env->envc].indent);
	HTMLlineproc1(tmp->ptr, h_env);
	set_space_to_prevchar(obuf->prevchar);
	return 1;
    case HTML_PRE:
	x = parsedtag_exists(tag, ATTR_FOR_TABLE);
	CLOSE_A;
	if (!(obuf->flag & RB_IGNORE_P)) {
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	    if (!x)
		do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
			     h_env->limit);
	}
	else
	    fillline(obuf, envs[h_env->envc].indent);
	obuf->flag |= (RB_PRE | RB_IGNORE_P);
	/* istr = str; */
	return 1;
    case HTML_N_PRE:
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	if (!(obuf->flag & RB_IGNORE_P)) {
	    do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
			 h_env->limit);
	    obuf->flag |= RB_IGNORE_P;
	    h_env->blank_lines++;
	}
	obuf->flag &= ~RB_PRE;
	close_anchor(h_env, obuf);
	return 1;
    case HTML_PRE_INT:
	i = obuf->line->length;
	append_tags(obuf);
	if (!(obuf->flag & RB_SPECIAL)) {
	    set_breakpoint(obuf, obuf->line->length - i);
	}
	obuf->flag |= RB_PRE_INT;
	return 0;
    case HTML_N_PRE_INT:
	push_tag(obuf, "</pre_int>", HTML_N_PRE_INT);
	obuf->flag &= ~RB_PRE_INT;
	if (!(obuf->flag & RB_SPECIAL) && obuf->pos > obuf->bp.pos) {
	    set_prevchar(obuf->prevchar, "", 0);
	    obuf->prev_ctype = PC_CTRL;
	}
	return 1;
    case HTML_NOBR:
	obuf->flag |= RB_NOBR;
	obuf->nobr_level++;
	return 0;
    case HTML_N_NOBR:
	if (obuf->nobr_level > 0)
	    obuf->nobr_level--;
	if (obuf->nobr_level == 0)
	    obuf->flag &= ~RB_NOBR;
	return 0;
    case HTML_PRE_PLAIN:
	CLOSE_A;
	if (!(obuf->flag & RB_IGNORE_P)) {
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	    do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
			 h_env->limit);
	}
	obuf->flag |= (RB_PRE | RB_IGNORE_P);
	return 1;
    case HTML_N_PRE_PLAIN:
	CLOSE_A;
	if (!(obuf->flag & RB_IGNORE_P)) {
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	    do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
			 h_env->limit);
	    obuf->flag |= RB_IGNORE_P;
	}
	obuf->flag &= ~RB_PRE;
	return 1;
    case HTML_LISTING:
    case HTML_XMP:
    case HTML_PLAINTEXT:
	CLOSE_A;
	if (!(obuf->flag & RB_IGNORE_P)) {
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	    do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
			 h_env->limit);
	}
	obuf->flag |= (RB_PLAIN | RB_IGNORE_P);
	switch (cmd) {
	case HTML_LISTING:
	    obuf->end_tag = HTML_N_LISTING;
	    break;
	case HTML_XMP:
	    obuf->end_tag = HTML_N_XMP;
	    break;
	case HTML_PLAINTEXT:
	    obuf->end_tag = MAX_HTMLTAG;
	    break;
	}
	return 1;
    case HTML_N_LISTING:
    case HTML_N_XMP:
	CLOSE_A;
	if (!(obuf->flag & RB_IGNORE_P)) {
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	    do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
			 h_env->limit);
	    obuf->flag |= RB_IGNORE_P;
	}
	obuf->flag &= ~RB_PLAIN;
	obuf->end_tag = 0;
	return 1;
    case HTML_SCRIPT:
	obuf->flag |= RB_SCRIPT;
	obuf->end_tag = HTML_N_SCRIPT;
	return 1;
    case HTML_STYLE:
	obuf->flag |= RB_STYLE;
	obuf->end_tag = HTML_N_STYLE;
	return 1;
    case HTML_N_SCRIPT:
	obuf->flag &= ~RB_SCRIPT;
	obuf->end_tag = 0;
	return 1;
    case HTML_N_STYLE:
	obuf->flag &= ~RB_STYLE;
	obuf->end_tag = 0;
	return 1;
    case HTML_A:
	if (obuf->anchor.url)
	    close_anchor(h_env, obuf);

	hseq = 0;

	if (parsedtag_get_value(tag, ATTR_HREF, &p))
	    obuf->anchor.url = Strnew_charp(p)->ptr;
	if (parsedtag_get_value(tag, ATTR_TARGET, &p))
	    obuf->anchor.target = Strnew_charp(p)->ptr;
	if (parsedtag_get_value(tag, ATTR_REFERER, &p))
	    obuf->anchor.referer = Strnew_charp(p)->ptr;
	if (parsedtag_get_value(tag, ATTR_TITLE, &p))
	    obuf->anchor.title = Strnew_charp(p)->ptr;
	if (parsedtag_get_value(tag, ATTR_ACCESSKEY, &p))
	    obuf->anchor.accesskey = (unsigned char)*p;
	if (parsedtag_get_value(tag, ATTR_HSEQ, &hseq))
	    obuf->anchor.hseq = hseq;

	if (hseq == 0 && obuf->anchor.url) {
	    obuf->anchor.hseq = cur_hseq;
	    tmp = process_anchor(tag, h_env->tagbuf->ptr);
	    push_tag(obuf, tmp->ptr, HTML_A);
	    if (displayLinkNumber)
		HTMLlineproc1(getLinkNumberStr(-1)->ptr, h_env);
	    return 1;
	}
	return 0;
    case HTML_N_A:
	close_anchor(h_env, obuf);
	return 1;
    case HTML_IMG:
	tmp = process_img(tag, h_env->limit);
	HTMLlineproc1(tmp->ptr, h_env);
	return 1;
    case HTML_IMG_ALT:
	if (parsedtag_get_value(tag, ATTR_SRC, &p))
	    obuf->img_alt = Strnew_charp(p);
#ifdef USE_IMAGE
	i = 0;
	if (parsedtag_get_value(tag, ATTR_TOP_MARGIN, &i)) {
	    if ((short)i > obuf->top_margin)
		obuf->top_margin = (short)i;
	}
	i = 0;
	if (parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &i)) {
	    if ((short)i > obuf->bottom_margin)
		obuf->bottom_margin = (short)i;
	}
#endif
	return 0;
    case HTML_N_IMG_ALT:
	if (obuf->img_alt) {
	    if (!close_effect0(obuf, HTML_IMG_ALT))
		push_tag(obuf, "</img_alt>", HTML_N_IMG_ALT);
	    obuf->img_alt = NULL;
	}
	return 1;
    case HTML_INPUT_ALT:
	i = 0;
	if (parsedtag_get_value(tag, ATTR_TOP_MARGIN, &i)) {
	    if ((short)i > obuf->top_margin)
		obuf->top_margin = (short)i;
	}
	i = 0;
	if (parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &i)) {
	    if ((short)i > obuf->bottom_margin)
		obuf->bottom_margin = (short)i;
	}
	if (parsedtag_get_value(tag, ATTR_HSEQ, &hseq)) {
	    obuf->input_alt.hseq = hseq;
	}
	if (parsedtag_get_value(tag, ATTR_FID, &i)) {
	    obuf->input_alt.fid = i;
	}
	if (parsedtag_get_value(tag, ATTR_TYPE, &p)) {
	    obuf->input_alt.type = Strnew_charp(p);
	}
	if (parsedtag_get_value(tag, ATTR_VALUE, &p)) {
	    obuf->input_alt.value = Strnew_charp(p);
	}
	if (parsedtag_get_value(tag, ATTR_NAME, &p)) {
	    obuf->input_alt.name = Strnew_charp(p);
	}
	obuf->input_alt.in = 1;
	return 0;
    case HTML_N_INPUT_ALT:
	if (obuf->input_alt.in) {
	    if (!close_effect0(obuf, HTML_INPUT_ALT))
		push_tag(obuf, "</input_alt>", HTML_N_INPUT_ALT);
	    obuf->input_alt.hseq = 0;
	    obuf->input_alt.fid = -1;
	    obuf->input_alt.in = 0;
	    obuf->input_alt.type = NULL;
	    obuf->input_alt.name = NULL;
	    obuf->input_alt.value = NULL;
	}
	return 1;
    case HTML_TABLE:
	close_anchor(h_env, obuf);
	obuf->table_level++;
	if (obuf->table_level >= MAX_TABLE)
	    break;
	w = BORDER_NONE;
	/* x: cellspacing, y: cellpadding */
	x = 2;
	y = 1;
	z = 0;
	width = 0;
	if (parsedtag_exists(tag, ATTR_BORDER)) {
	    if (parsedtag_get_value(tag, ATTR_BORDER, &w)) {
		if (w > 2)
		    w = BORDER_THICK;
		else if (w < 0) {	/* weird */
		    w = BORDER_THIN;
		}
	    }
	    else
		w = BORDER_THIN;
	}
	if (DisplayBorders && w == BORDER_NONE)
	    w = BORDER_THIN;
	if (parsedtag_get_value(tag, ATTR_WIDTH, &i)) {
	    if (obuf->table_level == 0)
		width = REAL_WIDTH(i, h_env->limit - envs[h_env->envc].indent);
	    else
		width = RELATIVE_WIDTH(i);
	}
	if (parsedtag_exists(tag, ATTR_HBORDER))
	    w = BORDER_NOWIN;
#define MAX_CELLSPACING 1000
#define MAX_CELLPADDING 1000
#define MAX_VSPACE 1000
	parsedtag_get_value(tag, ATTR_CELLSPACING, &x);
	parsedtag_get_value(tag, ATTR_CELLPADDING, &y);
	parsedtag_get_value(tag, ATTR_VSPACE, &z);
	if (x < 0)
	    x = 0;
	if (y < 0)
	    y = 0;
	if (z < 0)
	    z = 0;
	if (x > MAX_CELLSPACING)
	    x = MAX_CELLSPACING;
	if (y > MAX_CELLPADDING)
	    y = MAX_CELLPADDING;
	if (z > MAX_VSPACE)
	    z = MAX_VSPACE;
#ifdef ID_EXT
	parsedtag_get_value(tag, ATTR_ID, &id);
#endif				/* ID_EXT */
	tables[obuf->table_level] = begin_table(w, x, y, z);
#ifdef ID_EXT
	if (id != NULL)
	    tables[obuf->table_level]->id = Strnew_charp(id);
#endif				/* ID_EXT */
	table_mode[obuf->table_level].pre_mode = 0;
	table_mode[obuf->table_level].indent_level = 0;
	table_mode[obuf->table_level].nobr_level = 0;
	table_mode[obuf->table_level].caption = 0;
	table_mode[obuf->table_level].end_tag = 0;	/* HTML_UNKNOWN */
#ifndef TABLE_EXPAND
	tables[obuf->table_level]->total_width = width;
#else
	tables[obuf->table_level]->real_width = width;
	tables[obuf->table_level]->total_width = 0;
#endif
	return 1;
    case HTML_N_TABLE:
	/* should be processed in HTMLlineproc() */
	return 1;
    case HTML_CENTER:
	CLOSE_A;
	if (!(obuf->flag & (RB_PREMODE | RB_IGNORE_P)))
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	RB_SAVE_FLAG(obuf);
	RB_SET_ALIGN(obuf, RB_CENTER);
	return 1;
    case HTML_N_CENTER:
	CLOSE_A;
	if (!(obuf->flag & RB_PREMODE))
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	RB_RESTORE_FLAG(obuf);
	return 1;
    case HTML_DIV:
	CLOSE_A;
	if (!(obuf->flag & RB_IGNORE_P))
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	set_alignment(obuf, tag);
	return 1;
    case HTML_N_DIV:
	CLOSE_A;
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	RB_RESTORE_FLAG(obuf);
	return 1;
    case HTML_DIV_INT:
	CLOSE_P;
	if (!(obuf->flag & RB_IGNORE_P))
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	set_alignment(obuf, tag);
	return 1;
    case HTML_N_DIV_INT:
	CLOSE_P;
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	RB_RESTORE_FLAG(obuf);
	return 1;
    case HTML_FORM:
	CLOSE_A;
	if (!(obuf->flag & RB_IGNORE_P))
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	tmp = process_form(tag);
	if (tmp)
	    HTMLlineproc1(tmp->ptr, h_env);
	return 1;
    case HTML_N_FORM:
	CLOSE_A;
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	obuf->flag |= RB_IGNORE_P;
	process_n_form();
	return 1;
    case HTML_INPUT:
	close_anchor(h_env, obuf);
	tmp = process_input(tag);
       if (tmp)
           HTMLlineproc1(tmp->ptr, h_env);
       return 1;
    case HTML_BUTTON:
       tmp = process_button(tag);
       if (tmp)
           HTMLlineproc1(tmp->ptr, h_env);
       return 1;
    case HTML_N_BUTTON:
       tmp = process_n_button();
	if (tmp)
	    HTMLlineproc1(tmp->ptr, h_env);
	return 1;
    case HTML_SELECT:
	close_anchor(h_env, obuf);
	tmp = process_select(tag);
	if (tmp)
	    HTMLlineproc1(tmp->ptr, h_env);
	obuf->flag |= RB_INSELECT;
	obuf->end_tag = HTML_N_SELECT;
	return 1;
    case HTML_N_SELECT:
	obuf->flag &= ~RB_INSELECT;
	obuf->end_tag = 0;
	tmp = process_n_select();
	if (tmp)
	    HTMLlineproc1(tmp->ptr, h_env);
	return 1;
    case HTML_OPTION:
	/* nothing */
	return 1;
    case HTML_TEXTAREA:
	close_anchor(h_env, obuf);
	tmp = process_textarea(tag, h_env->limit);
	if (tmp)
	    HTMLlineproc1(tmp->ptr, h_env);
	obuf->flag |= RB_INTXTA;
	obuf->end_tag = HTML_N_TEXTAREA;
	return 1;
    case HTML_N_TEXTAREA:
	obuf->flag &= ~RB_INTXTA;
	obuf->end_tag = 0;
	tmp = process_n_textarea();
	if (tmp)
	    HTMLlineproc1(tmp->ptr, h_env);
	return 1;
    case HTML_ISINDEX:
	p = "";
	q = "!CURRENT_URL!";
	parsedtag_get_value(tag, ATTR_PROMPT, &p);
	parsedtag_get_value(tag, ATTR_ACTION, &q);
	tmp = Strnew_m_charp("<form method=get action=\"",
			     html_quote(q),
			     "\">",
			     html_quote(p),
			     "<input type=text name=\"\" accept></form>",
			     NULL);
	HTMLlineproc1(tmp->ptr, h_env);
	return 1;
    case HTML_META:
	p = q = r = NULL;
	parsedtag_get_value(tag, ATTR_HTTP_EQUIV, &p);
	parsedtag_get_value(tag, ATTR_CONTENT, &q);
#ifdef USE_M17N
	parsedtag_get_value(tag, ATTR_CHARSET, &r);
	if (r) {
	    /* <meta charset=""> */
	    SKIP_BLANKS(r);
	    meta_charset = wc_guess_charset(r, 0);
	}
	else
	if (p && q && !strcasecmp(p, "Content-Type") &&
	    (q = strcasestr(q, "charset")) != NULL) {
	    q += 7;
	    SKIP_BLANKS(q);
	    if (*q == '=') {
		q++;
		SKIP_BLANKS(q);
		meta_charset = wc_guess_charset(q, 0);
	    }
	}
	else
#endif
	if (p && q && !strcasecmp(p, "refresh")) {
	    int refresh_interval;
	    tmp = NULL;
	    refresh_interval = getMetaRefreshParam(q, &tmp);
	    if (tmp) {
		q = html_quote(tmp->ptr);
		tmp = Sprintf("Refresh (%d sec) <a href=\"%s\">%s</a>",
			      refresh_interval, q, q);
	    }
	    else if (refresh_interval > 0)
		tmp = Sprintf("Refresh (%d sec)", refresh_interval);
	    if (tmp) {
		HTMLlineproc1(tmp->ptr, h_env);
		do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
			     h_env->limit);
		if (!is_redisplay &&
		    !((obuf->flag & RB_NOFRAMES) && RenderFrame)) {
		    tag->need_reconstruct = TRUE;
		    return 0;
		}
	    }
	}
	return 1;
    case HTML_BASE:
#if defined(USE_M17N) || defined(USE_IMAGE)
	p = NULL;
	if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
	    cur_baseURL = New(ParsedURL);
	    parseURL(p, cur_baseURL, NULL);
	}
#endif
    case HTML_MAP:
    case HTML_N_MAP:
    case HTML_AREA:
	return 0;
    case HTML_DEL:
	switch (displayInsDel) {
	case DISPLAY_INS_DEL_SIMPLE:
	    obuf->flag |= RB_DEL;
	    break;
	case DISPLAY_INS_DEL_NORMAL:
	    HTMLlineproc1("<U>[DEL:</U>", h_env);
	    break;
	case DISPLAY_INS_DEL_FONTIFY:
	    obuf->in_strike++;
	    if (obuf->in_strike == 1) {
		push_tag(obuf, "<s>", HTML_S);
	    }
	    break;
	}
	return 1;
    case HTML_N_DEL:
	switch (displayInsDel) {
	case DISPLAY_INS_DEL_SIMPLE:
	    obuf->flag &= ~RB_DEL;
	    break;
	case DISPLAY_INS_DEL_NORMAL:
	    HTMLlineproc1("<U>:DEL]</U>", h_env);
	case DISPLAY_INS_DEL_FONTIFY:
	    if (obuf->in_strike == 0)
		return 1;
	    if (obuf->in_strike == 1 && close_effect0(obuf, HTML_S))
		obuf->in_strike = 0;
	    if (obuf->in_strike > 0) {
		obuf->in_strike--;
		if (obuf->in_strike == 0) {
		    push_tag(obuf, "</s>", HTML_N_S);
		}
	    }
	    break;
	}
	return 1;
    case HTML_S:
	switch (displayInsDel) {
	case DISPLAY_INS_DEL_SIMPLE:
	    obuf->flag |= RB_S;
	    break;
	case DISPLAY_INS_DEL_NORMAL:
	    HTMLlineproc1("<U>[S:</U>", h_env);
	    break;
	case DISPLAY_INS_DEL_FONTIFY:
	    obuf->in_strike++;
	    if (obuf->in_strike == 1) {
		push_tag(obuf, "<s>", HTML_S);
	    }
	    break;
	}
	return 1;
    case HTML_N_S:
	switch (displayInsDel) {
	case DISPLAY_INS_DEL_SIMPLE:
	    obuf->flag &= ~RB_S;
	    break;
	case DISPLAY_INS_DEL_NORMAL:
	    HTMLlineproc1("<U>:S]</U>", h_env);
	    break;
	case DISPLAY_INS_DEL_FONTIFY:
	    if (obuf->in_strike == 0)
		return 1;
	    if (obuf->in_strike == 1 && close_effect0(obuf, HTML_S))
		obuf->in_strike = 0;
	    if (obuf->in_strike > 0) {
		obuf->in_strike--;
		if (obuf->in_strike == 0) {
		    push_tag(obuf, "</s>", HTML_N_S);
		}
	    }
	}
	return 1;
    case HTML_INS:
	switch (displayInsDel) {
	case DISPLAY_INS_DEL_SIMPLE:
	    break;
	case DISPLAY_INS_DEL_NORMAL:
	    HTMLlineproc1("<U>[INS:</U>", h_env);
	    break;
	case DISPLAY_INS_DEL_FONTIFY:
	    obuf->in_ins++;
	    if (obuf->in_ins == 1) {
		push_tag(obuf, "<ins>", HTML_INS);
	    }
	    break;
	}
	return 1;
    case HTML_N_INS:
	switch (displayInsDel) {
	case DISPLAY_INS_DEL_SIMPLE:
	    break;
	case DISPLAY_INS_DEL_NORMAL:
	    HTMLlineproc1("<U>:INS]</U>", h_env);
	    break;
	case DISPLAY_INS_DEL_FONTIFY:
	    if (obuf->in_ins == 0)
		return 1;
	    if (obuf->in_ins == 1 && close_effect0(obuf, HTML_INS))
		obuf->in_ins = 0;
	    if (obuf->in_ins > 0) {
		obuf->in_ins--;
		if (obuf->in_ins == 0) {
		    push_tag(obuf, "</ins>", HTML_N_INS);
		}
	    }
	    break;
	}
	return 1;
    case HTML_SUP:
	if (!(obuf->flag & (RB_DEL | RB_S)))
	    HTMLlineproc1("^", h_env);
	return 1;
    case HTML_N_SUP:
	return 1;
    case HTML_SUB:
	if (!(obuf->flag & (RB_DEL | RB_S)))
	    HTMLlineproc1("[", h_env);
	return 1;
    case HTML_N_SUB:
	if (!(obuf->flag & (RB_DEL | RB_S)))
	    HTMLlineproc1("]", h_env);
	return 1;
    case HTML_FONT:
    case HTML_N_FONT:
    case HTML_NOP:
	return 1;
    case HTML_BGSOUND:
	if (view_unseenobject) {
	    if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
		Str s;
		q = html_quote(p);
		s = Sprintf("<A HREF=\"%s\">bgsound(%s)</A>", q, q);
		HTMLlineproc1(s->ptr, h_env);
	    }
	}
	return 1;
    case HTML_EMBED:
	if (view_unseenobject) {
	    if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
		Str s;
		q = html_quote(p);
		s = Sprintf("<A HREF=\"%s\">embed(%s)</A>", q, q);
		HTMLlineproc1(s->ptr, h_env);
	    }
	}
	return 1;
    case HTML_APPLET:
	if (view_unseenobject) {
	    if (parsedtag_get_value(tag, ATTR_ARCHIVE, &p)) {
		Str s;
		q = html_quote(p);
		s = Sprintf("<A HREF=\"%s\">applet archive(%s)</A>", q, q);
		HTMLlineproc1(s->ptr, h_env);
	    }
	}
	return 1;
    case HTML_BODY:
	if (view_unseenobject) {
	    if (parsedtag_get_value(tag, ATTR_BACKGROUND, &p)) {
		Str s;
		q = html_quote(p);
		s = Sprintf("<IMG SRC=\"%s\" ALT=\"bg image(%s)\"><BR>", q, q);
		HTMLlineproc1(s->ptr, h_env);
	    }
	}
    case HTML_N_HEAD:
	if (obuf->flag & RB_TITLE)
	    HTMLlineproc1("</title>", h_env);
    case HTML_HEAD:
    case HTML_N_BODY:
	return 1;
    default:
	/* obuf->prevchar = '\0'; */
	return 0;
    }
    /* not reached */
    return 0;
}