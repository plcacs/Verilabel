SaltTextAway(XtermWidget xw,
	     int which,
	     CELL *cellc,
	     CELL *cell)
{
    TScreen *screen = TScreenOf(xw);
    SelectedCells *scp;
    int i;
    int eol;
    int need = 0;
    Char *line;
    Char *lp;
    CELL first = *cellc;
    CELL last = *cell;

    if (which < 0 || which >= MAX_SELECTIONS) {
	TRACE(("SaltTextAway - which selection?\n"));
	return;
    }
    scp = &(screen->selected_cells[which]);

    TRACE(("SaltTextAway which=%d, first=%d,%d, last=%d,%d\n",
	   which, first.row, first.col, last.row, last.col));

    if (isSameRow(&first, &last) && first.col > last.col) {
	int tmp;
	EXCHANGE(first.col, last.col, tmp);
    }

    --last.col;
    /* first we need to know how long the string is before we can save it */

    if (isSameRow(&last, &first)) {
	need = Length(screen, first.row, first.col, last.col);
    } else {			/* two cases, cut is on same line, cut spans multiple lines */
	need += Length(screen, first.row, first.col, screen->max_col) + 1;
	for (i = first.row + 1; i < last.row; i++)
	    need += Length(screen, i, 0, screen->max_col) + 1;
	if (last.col >= 0)
	    need += Length(screen, last.row, 0, last.col);
    }

    /* UTF-8 may require more space */
    if_OPT_WIDE_CHARS(screen, {
	need *= 4;
    });

    /* now get some memory to save it in */
    if (need < 0)
	return;

    if (scp->data_limit <= (unsigned) need) {
	if ((line = (Char *) malloc((size_t) need + 1)) == 0)
	    SysError(ERROR_BMALLOC2);
	free(scp->data_buffer);
	scp->data_buffer = line;
	scp->data_limit = (size_t) (need + 1);
    } else {
	line = scp->data_buffer;
    }

    if (line == 0)
	return;

    line[need] = '\0';		/* make sure it is null terminated */
    lp = line;			/* lp points to where to save the text */
    if (isSameRow(&last, &first)) {
	lp = SaveText(screen, last.row, first.col, last.col, lp, &eol);
    } else {
	lp = SaveText(screen, first.row, first.col, screen->max_col, lp, &eol);
	if (eol)
	    *lp++ = '\n';	/* put in newline at end of line */
	for (i = first.row + 1; i < last.row; i++) {
	    lp = SaveText(screen, i, 0, screen->max_col, lp, &eol);
	    if (eol)
		*lp++ = '\n';
	}
	if (last.col >= 0)
	    lp = SaveText(screen, last.row, 0, last.col, lp, &eol);
    }
    *lp = '\0';			/* make sure we have end marked */

    TRACE(("Salted TEXT:%u:%s\n", (unsigned) (lp - line),
	   visibleChars(line, (unsigned) (lp - line))));

    scp->data_length = (size_t) (lp - line);
}