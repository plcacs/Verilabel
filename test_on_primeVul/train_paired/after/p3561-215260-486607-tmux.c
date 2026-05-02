input_csi_dispatch_sgr_colon(struct input_ctx *ictx, u_int i)
{
	struct grid_cell	*gc = &ictx->cell.cell;
	char			*s = ictx->param_list[i].str, *copy, *ptr, *out;
	int			 p[8];
	u_int			 n;
	const char		*errstr;

	for (n = 0; n < nitems(p); n++)
		p[n] = -1;
	n = 0;

	ptr = copy = xstrdup(s);
	while ((out = strsep(&ptr, ":")) != NULL) {
		if (*out != '\0') {
			p[n++] = strtonum(out, 0, INT_MAX, &errstr);
			if (errstr != NULL || n == nitems(p)) {
				free(copy);
				return;
			}
		} else {
			n++;
			if (n == nitems(p)) {
				free(copy);
				return;
			}
		}
		log_debug("%s: %u = %d", __func__, n - 1, p[n - 1]);
	}
	free(copy);

	if (n == 0)
		return;
	if (p[0] == 4) {
		if (n != 2)
			return;
		switch (p[1]) {
		case 0:
			gc->attr &= ~GRID_ATTR_ALL_UNDERSCORE;
			break;
		case 1:
			gc->attr &= ~GRID_ATTR_ALL_UNDERSCORE;
			gc->attr |= GRID_ATTR_UNDERSCORE;
			break;
		case 2:
			gc->attr &= ~GRID_ATTR_ALL_UNDERSCORE;
			gc->attr |= GRID_ATTR_UNDERSCORE_2;
			break;
		case 3:
			gc->attr &= ~GRID_ATTR_ALL_UNDERSCORE;
			gc->attr |= GRID_ATTR_UNDERSCORE_3;
			break;
		case 4:
			gc->attr &= ~GRID_ATTR_ALL_UNDERSCORE;
			gc->attr |= GRID_ATTR_UNDERSCORE_4;
			break;
		case 5:
			gc->attr &= ~GRID_ATTR_ALL_UNDERSCORE;
			gc->attr |= GRID_ATTR_UNDERSCORE_5;
			break;
		}
		return;
	}
	if (n < 2 || (p[0] != 38 && p[0] != 48 && p[0] != 58))
		return;
	switch (p[1]) {
	case 2:
		if (n < 3)
			break;
		if (n == 5)
			i = 2;
		else
			i = 3;
		if (n < i + 3)
			break;
		input_csi_dispatch_sgr_rgb_do(ictx, p[0], p[i], p[i + 1],
		    p[i + 2]);
		break;
	case 5:
		if (n < 3)
			break;
		input_csi_dispatch_sgr_256_do(ictx, p[0], p[2]);
		break;
	}
}