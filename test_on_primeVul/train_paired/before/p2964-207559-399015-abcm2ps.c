static void set_k_acc(struct SYMBOL *s)
{
	int i, j, nacc;
	char accs[8], pits[8];
	static char sharp_tb[8] = {26, 23, 27, 24, 21, 25, 22};
	static char flat_tb[8] = {22, 25, 21, 24, 20, 23, 26};

	if (s->u.key.sf > 0) {
		for (nacc = 0; nacc < s->u.key.sf; nacc++) {
			accs[nacc] = A_SH;
			pits[nacc] = sharp_tb[nacc];
		}
	} else {
		for (nacc = 0; nacc < -s->u.key.sf; nacc++) {
			accs[nacc] = A_FT;
			pits[nacc] = flat_tb[nacc];
		}
	}
	for (i = 0; i < s->u.key.nacc; i++) {
		for (j = 0; j < nacc; j++) {
//			if ((pits[j] - s->u.key.pits[i]) % 7 == 0) {
			if (pits[j] == s->u.key.pits[i]) {
				accs[j] = s->u.key.accs[i];
				break;
			}
		}
		if (j == nacc) {
			accs[j] = s->u.key.accs[i];
			pits[j] = s->u.key.pits[i];
			nacc++;		/* cannot overflow */
		}
	}
	for (i = 0; i < nacc; i++) {
		s->u.key.accs[i] = accs[i];
		s->u.key.pits[i] = pits[i];
	}
	s->u.key.nacc = nacc;
}