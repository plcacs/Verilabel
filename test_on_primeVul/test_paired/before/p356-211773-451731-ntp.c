cookedprint(
	int datatype,
	int length,
	const char *data,
	int status,
	int quiet,
	FILE *fp
	)
{
	char *name;
	char *value;
	char output_raw;
	int fmt;
	l_fp lfp;
	sockaddr_u hval;
	u_long uval;
	int narr;
	size_t len;
	l_fp lfparr[8];
	char b[12];
	char bn[2 * MAXVARLEN];
	char bv[2 * MAXVALLEN];

	UNUSED_ARG(datatype);

	if (!quiet)
		fprintf(fp, "status=%04x %s,\n", status,
			statustoa(datatype, status));

	startoutput();
	while (nextvar(&length, &data, &name, &value)) {
		fmt = varfmt(name);
		output_raw = 0;
		switch (fmt) {

		case PADDING:
			output_raw = '*';
			break;

		case TS:
			if (!decodets(value, &lfp))
				output_raw = '?';
			else
				output(fp, name, prettydate(&lfp));
			break;

		case HA:	/* fallthru */
		case NA:
			if (!decodenetnum(value, &hval)) {
				output_raw = '?';
			} else if (fmt == HA){
				output(fp, name, nntohost(&hval));
			} else {
				output(fp, name, stoa(&hval));
			}
			break;

		case RF:
			if (decodenetnum(value, &hval)) {
				if (ISREFCLOCKADR(&hval))
					output(fp, name,
					       refnumtoa(&hval));
				else
					output(fp, name, stoa(&hval));
			} else if (strlen(value) <= 4) {
				output(fp, name, value);
			} else {
				output_raw = '?';
			}
			break;

		case LP:
			if (!decodeuint(value, &uval) || uval > 3) {
				output_raw = '?';
			} else {
				b[0] = (0x2 & uval)
					   ? '1'
					   : '0';
				b[1] = (0x1 & uval)
					   ? '1'
					   : '0';
				b[2] = '\0';
				output(fp, name, b);
			}
			break;

		case OC:
			if (!decodeuint(value, &uval)) {
				output_raw = '?';
			} else {
				snprintf(b, sizeof(b), "%03lo", uval);
				output(fp, name, b);
			}
			break;

		case AR:
			if (!decodearr(value, &narr, lfparr))
				output_raw = '?';
			else
				outputarr(fp, name, narr, lfparr);
			break;

		case FX:
			if (!decodeuint(value, &uval))
				output_raw = '?';
			else
				output(fp, name, tstflags(uval));
			break;

		default:
			fprintf(stderr, "Internal error in cookedprint, %s=%s, fmt %d\n",
				name, value, fmt);
			output_raw = '?';
			break;
		}

		if (output_raw != 0) {
			atoascii(name, MAXVARLEN, bn, sizeof(bn));
			atoascii(value, MAXVALLEN, bv, sizeof(bv));
			if (output_raw != '*') {
				len = strlen(bv);
				bv[len] = output_raw;
				bv[len+1] = '\0';
			}
			output(fp, bn, bv);
		}
	}
	endoutput(fp);
}