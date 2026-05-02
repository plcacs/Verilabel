static void record_init(node_t * n)
{
    field_t *info;
    pointf ul, sz;
    int flip, len;
    char *textbuf;		/* temp buffer for storing labels */
    int sides = BOTTOM | RIGHT | TOP | LEFT;

    /* Always use rankdir to determine how records are laid out */
    flip = NOT(GD_realflip(agraphof(n)));
    reclblp = ND_label(n)->text;
    len = strlen(reclblp);
    /* For some forgotten reason, an empty label is parsed into a space, so
     * we need at least two bytes in textbuf.
     */
    len = MAX(len, 1);
    textbuf = N_NEW(len + 1, char);
    if (!(info = parse_reclbl(n, flip, TRUE, textbuf))) {
	agerr(AGERR, "bad label format %s\n", ND_label(n)->text);
	reclblp = "\\N";
	info = parse_reclbl(n, flip, TRUE, textbuf);
    }
    free(textbuf);
    size_reclbl(n, info);
    sz.x = POINTS(ND_width(n));
    sz.y = POINTS(ND_height(n));
    if (mapbool(late_string(n, N_fixed, "false"))) {
	if ((sz.x < info->size.x) || (sz.y < info->size.y)) {
/* should check that the record really won't fit, e.g., there may be no text.
			agerr(AGWARN, "node '%s' size may be too small\n", agnameof(n));
*/
	}
    } else {
	sz.x = MAX(info->size.x, sz.x);
	sz.y = MAX(info->size.y, sz.y);
    }
    resize_reclbl(info, sz, mapbool(late_string(n, N_nojustify, "false")));
    ul = pointfof(-sz.x / 2., sz.y / 2.);	/* FIXME - is this still true:    suspected to introduce ronding error - see Kluge below */
    pos_reclbl(info, ul, sides);
    ND_width(n) = PS2INCH(info->size.x);
    ND_height(n) = PS2INCH(info->size.y + 1);	/* Kluge!!  +1 to fix rounding diff between layout and rendering 
						   otherwise we can get -1 coords in output */
    ND_shape_info(n) = (void *) info;
}