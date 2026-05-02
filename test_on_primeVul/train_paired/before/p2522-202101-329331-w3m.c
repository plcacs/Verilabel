addMultirowsForm(Buffer *buf, AnchorList *al)
{
    int i, j, k, col, ecol, pos;
    Anchor a_form, *a;
    Line *l, *ls;

    if (al == NULL || al->nanchor == 0)
	return;
    for (i = 0; i < al->nanchor; i++) {
	a_form = al->anchors[i];
	al->anchors[i].rows = 1;
	if (a_form.hseq < 0 || a_form.rows <= 1)
	    continue;
	for (l = buf->firstLine; l != NULL; l = l->next) {
	    if (l->linenumber == a_form.y)
		break;
	}
	if (!l)
	    continue;
	if (a_form.y == a_form.start.line)
	    ls = l;
	else {
	    for (ls = l; ls != NULL;
		 ls = (a_form.y < a_form.start.line) ? ls->next : ls->prev) {
		if (ls->linenumber == a_form.start.line)
		    break;
	    }
	    if (!ls)
		continue;
	}
	col = COLPOS(ls, a_form.start.pos);
	ecol = COLPOS(ls, a_form.end.pos);
	for (j = 0; l && j < a_form.rows; l = l->next, j++) {
	    pos = columnPos(l, col);
	    if (j == 0) {
		buf->hmarklist->marks[a_form.hseq].line = l->linenumber;
		buf->hmarklist->marks[a_form.hseq].pos = pos;
	    }
	    if (a_form.start.line == l->linenumber)
		continue;
	    buf->formitem = putAnchor(buf->formitem, a_form.url,
				      a_form.target, &a, NULL, NULL, '\0',
				      l->linenumber, pos);
	    a->hseq = a_form.hseq;
	    a->y = a_form.y;
	    a->end.pos = pos + ecol - col;
	    l->lineBuf[pos - 1] = '[';
	    l->lineBuf[a->end.pos] = ']';
	    for (k = pos; k < a->end.pos; k++)
		l->propBuf[k] |= PE_FORM;
	}
    }
}