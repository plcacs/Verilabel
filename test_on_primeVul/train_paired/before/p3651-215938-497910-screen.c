static void MScrollV(Window *p, int n, int ys, int ye, int bce)
{
	int i, cnt1, cnt2;
	struct mline tmp[256];
	struct mline *ml;

	if (n == 0)
		return;
	if (n > 0) {
		if (n > 256) {
			MScrollV(p, n - 256, ys, ye, bce);
			n = 256;
		}
		if (ye - ys + 1 < n)
			n = ye - ys + 1;
		if (compacthist) {
			ye = MFindUsedLine(p, ye, ys);
			if (ye - ys + 1 < n)
				n = ye - ys + 1;
			if (n <= 0)
				return;
		}
		/* Clear lines */
		ml = p->w_mlines + ys;
		for (i = ys; i < ys + n; i++, ml++) {
			if (ys == p->w_top)
				WAddLineToHist(p, ml);
			if (ml->attr != null)
				free(ml->attr);
			ml->attr = null;
			if (ml->font != null)
				free(ml->font);
			ml->font = null;
			if (ml->fontx != null)
				free(ml->fontx);
			ml->fontx = null;
			if (ml->colorbg != null)
				free(ml->colorbg);
			ml->colorbg = null;
			if (ml->colorfg != null)
				free(ml->colorfg);
			ml->colorfg = null;
			memmove(ml->image, blank, (p->w_width + 1) * 4);
			if (bce)
				MBceLine(p, i, 0, p->w_width, bce);
		}
		/* switch 'em over */
		cnt1 = n * sizeof(struct mline);
		cnt2 = (ye - ys + 1 - n) * sizeof(struct mline);
		if (cnt1 && cnt2)
			Scroll((char *)(p->w_mlines + ys), cnt1, cnt2, (char *)tmp);
	} else {
		if (n < -256) {
			MScrollV(p, n + 256, ys, ye, bce);
			n = -256;
		}
		n = -n;
		if (ye - ys + 1 < n)
			n = ye - ys + 1;

		ml = p->w_mlines + ye;
		/* Clear lines */
		for (i = ye; i > ye - n; i--, ml--) {
			if (ml->attr != null)
				free(ml->attr);
			ml->attr = null;
			if (ml->font != null)
				free(ml->font);
			ml->font = null;
			if (ml->fontx != null)
				free(ml->fontx);
			ml->fontx = null;
			if (ml->colorbg != null)
				free(ml->colorbg);
			ml->colorbg = null;
			if (ml->colorfg != null)
				free(ml->colorfg);
			ml->colorfg = null;
			memmove(ml->image, blank, (p->w_width + 1) * 4);
			if (bce)
				MBceLine(p, i, 0, p->w_width, bce);
		}
		cnt1 = n * sizeof(struct mline);
		cnt2 = (ye - ys + 1 - n) * sizeof(struct mline);
		if (cnt1 && cnt2)
			Scroll((char *)(p->w_mlines + ys), cnt2, cnt1, (char *)tmp);
	}
}