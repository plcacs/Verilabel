parse_key_constraints(struct sshbuf *m, struct sshkey *k, time_t *deathp,
    u_int *secondsp, int *confirmp, char **sk_providerp)
{
	u_char ctype;
	int r;
	u_int seconds, maxsign = 0;

	while (sshbuf_len(m)) {
		if ((r = sshbuf_get_u8(m, &ctype)) != 0) {
			error_fr(r, "parse constraint type");
			goto out;
		}
		switch (ctype) {
		case SSH_AGENT_CONSTRAIN_LIFETIME:
			if (*deathp != 0) {
				error_f("lifetime already set");
				r = SSH_ERR_INVALID_FORMAT;
				goto out;
			}
			if ((r = sshbuf_get_u32(m, &seconds)) != 0) {
				error_fr(r, "parse lifetime constraint");
				goto out;
			}
			*deathp = monotime() + seconds;
			*secondsp = seconds;
			break;
		case SSH_AGENT_CONSTRAIN_CONFIRM:
			if (*confirmp != 0) {
				error_f("confirm already set");
				r = SSH_ERR_INVALID_FORMAT;
				goto out;
			}
			*confirmp = 1;
			break;
		case SSH_AGENT_CONSTRAIN_MAXSIGN:
			if (k == NULL) {
				error_f("maxsign not valid here");
				r = SSH_ERR_INVALID_FORMAT;
				goto out;
			}
			if (maxsign != 0) {
				error_f("maxsign already set");
				r = SSH_ERR_INVALID_FORMAT;
				goto out;
			}
			if ((r = sshbuf_get_u32(m, &maxsign)) != 0) {
				error_fr(r, "parse maxsign constraint");
				goto out;
			}
			if ((r = sshkey_enable_maxsign(k, maxsign)) != 0) {
				error_fr(r, "enable maxsign");
				goto out;
			}
			break;
		case SSH_AGENT_CONSTRAIN_EXTENSION:
			if ((r = parse_key_constraint_extension(m,
			    sk_providerp)) != 0)
				goto out; /* error already logged */
			break;
		default:
			error_f("Unknown constraint %d", ctype);
			r = SSH_ERR_FEATURE_UNSUPPORTED;
			goto out;
		}
	}
	/* success */
	r = 0;
 out:
	return r;
}