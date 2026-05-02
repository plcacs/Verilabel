shiftAnchorPosition(AnchorList *al, HmarkerList *hl, int line, int pos,
		    int shift)
{
    Anchor *a;
    size_t b, e, s = 0;
    int cmp;

    if (al == NULL || al->nanchor == 0)
	return;

    s = al->nanchor / 2;
    for (b = 0, e = al->nanchor - 1; b <= e; s = (b + e + 1) / 2) {
	a = &al->anchors[s];
	cmp = onAnchor(a, line, pos);
	if (cmp == 0)
	    break;
	else if (cmp > 0)
	    b = s + 1;
	else if (s == 0)
	    break;
	else
	    e = s - 1;
    }
    for (; s < al->nanchor; s++) {
	a = &al->anchors[s];
	if (a->start.line > line)
	    break;
	if (a->start.pos > pos) {
	    a->start.pos += shift;
	    if (hl->marks[a->hseq].line == line)
		hl->marks[a->hseq].pos = a->start.pos;
	}
	if (a->end.pos >= pos)
	    a->end.pos += shift;
    }
}