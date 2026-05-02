do_refill(struct table *tbl, int row, int col, int maxlimit)
{
    TextList *orgdata;
    TextListItem *l;
    struct readbuffer obuf;
    struct html_feed_environ h_env;
    struct environment envs[MAX_ENV_LEVEL];
    int colspan, icell;

    if (tbl->tabdata[row] == NULL || tbl->tabdata[row][col] == NULL)
	return;
    orgdata = (TextList *)tbl->tabdata[row][col];
    tbl->tabdata[row][col] = newGeneralList();

    init_henv(&h_env, &obuf, envs, MAX_ENV_LEVEL,
	      (TextLineList *)tbl->tabdata[row][col],
	      get_spec_cell_width(tbl, row, col), 0);
    obuf.flag |= RB_INTABLE;
    if (h_env.limit > maxlimit)
	h_env.limit = maxlimit;
    if (tbl->border_mode != BORDER_NONE && tbl->vcellpadding > 0)
	do_blankline(&h_env, &obuf, 0, 0, h_env.limit);
    for (l = orgdata->first; l != NULL; l = l->next) {
	if (TAG_IS(l->ptr, "<table_alt", 10)) {
	    int id = -1;
	    char *p = l->ptr;
	    struct parsed_tag *tag;
	    if ((tag = parse_tag(&p, TRUE)) != NULL)
		parsedtag_get_value(tag, ATTR_TID, &id);
	    if (id >= 0 && id < tbl->ntable) {
		int alignment;
		TextLineListItem *ti;
		struct table *t = tbl->tables[id].ptr;
		int limit = tbl->tables[id].indent + t->total_width;
		tbl->tables[id].ptr = NULL;
		save_fonteffect(&h_env, h_env.obuf);
		flushline(&h_env, &obuf, 0, 2, h_env.limit);
		if (t->vspace > 0 && !(obuf.flag & RB_IGNORE_P))
		    do_blankline(&h_env, &obuf, 0, 0, h_env.limit);
		if (RB_GET_ALIGN(h_env.obuf) == RB_CENTER)
		    alignment = ALIGN_CENTER;
		else if (RB_GET_ALIGN(h_env.obuf) == RB_RIGHT)
		    alignment = ALIGN_RIGHT;
		else
		    alignment = ALIGN_LEFT;

		if (alignment != ALIGN_LEFT) {
		    for (ti = tbl->tables[id].buf->first;
			 ti != NULL; ti = ti->next)
			align(ti->ptr, h_env.limit, alignment);
		}
		appendTextLineList(h_env.buf, tbl->tables[id].buf);
		if (h_env.maxlimit < limit)
		    h_env.maxlimit = limit;
		restore_fonteffect(&h_env, h_env.obuf);
		obuf.flag &= ~RB_IGNORE_P;
		h_env.blank_lines = 0;
		if (t->vspace > 0) {
		    do_blankline(&h_env, &obuf, 0, 0, h_env.limit);
		    obuf.flag |= RB_IGNORE_P;
		}
	    }
	}
	else
	    HTMLlineproc1(l->ptr, &h_env);
    }
    if (obuf.status != R_ST_NORMAL) {
	obuf.status = R_ST_EOL;
	HTMLlineproc1("\n", &h_env);
    }
    completeHTMLstream(&h_env, &obuf);
    flushline(&h_env, &obuf, 0, 2, h_env.limit);
    if (tbl->border_mode == BORDER_NONE) {
	int rowspan = table_rowspan(tbl, row, col);
	if (row + rowspan <= tbl->maxrow) {
	    if (tbl->vcellpadding > 0 && !(obuf.flag & RB_IGNORE_P))
		do_blankline(&h_env, &obuf, 0, 0, h_env.limit);
	}
	else {
	    if (tbl->vspace > 0)
		purgeline(&h_env);
	}
    }
    else {
	if (tbl->vcellpadding > 0) {
	    if (!(obuf.flag & RB_IGNORE_P))
		do_blankline(&h_env, &obuf, 0, 0, h_env.limit);
	}
	else
	    purgeline(&h_env);
    }
    if ((colspan = table_colspan(tbl, row, col)) > 1) {
	struct table_cell *cell = &tbl->cell;
	int k;
	k = bsearch_2short(colspan, cell->colspan, col, cell->col, MAXCOL,
			   cell->index, cell->maxcell + 1);
	icell = cell->index[k];
	if (cell->minimum_width[icell] < h_env.maxlimit)
	    cell->minimum_width[icell] = h_env.maxlimit;
    }
    else {
	if (tbl->minimum_width[col] < h_env.maxlimit)
	    tbl->minimum_width[col] = h_env.maxlimit;
    }
}