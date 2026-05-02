parse_key_constraints(struct sshbuf *m, struct sshkey *k, time_t *deathp,
    u_int *secondsp, int *confirmp, char **sk_providerp)
{
	u_char ctype;
	int r;
	u_int seconds, maxsign = 0;
	char *ext_name = NULL;
	struct sshbuf *b = NULL;

	while (sshbuf_len(m)) {
		if ((r = sshbuf_get_u8(m, &ctype)) != 0) {
			error_fr(r, "parse constraint type");
			goto err;
		}
		switch (ctype) {
		case SSH_AGENT_CONSTRAIN_LIFETIME:
			if (*deathp != 0) {
				error_f("lifetime already set");
				goto err;
			}
			if ((r = sshbuf_get_u32(m, &seconds)) != 0) {
				error_fr(r, "parse lifetime constraint");
				goto err;
			}
			*deathp = monotime() + seconds;
			*secondsp = seconds;
			break;
		case SSH_AGENT_CONSTRAIN_CONFIRM:
			if (*confirmp != 0) {
				error_f("confirm already set");
				goto err;
			}
			*confirmp = 1;
			break;
		case SSH_AGENT_CONSTRAIN_MAXSIGN:
			if (k == NULL) {
				error_f("maxsign not valid here");
				goto err;
			}
			if (maxsign != 0) {
				error_f("maxsign already set");
				goto err;
			}
			if ((r = sshbuf_get_u32(m, &maxsign)) != 0) {
				error_fr(r, "parse maxsign constraint");
				goto err;
			}
			if ((r = sshkey_enable_maxsign(k, maxsign)) != 0) {
				error_fr(r, "enable maxsign");
				goto err;
			}
			break;
		case SSH_AGENT_CONSTRAIN_EXTENSION:
			if ((r = sshbuf_get_cstring(m, &ext_name, NULL)) != 0) {
				error_fr(r, "parse constraint extension");
				goto err;
			}
			debug_f("constraint ext %s", ext_name);
			if (strcmp(ext_name, "sk-provider@openssh.com") == 0) {
				if (sk_providerp == NULL) {
					error_f("%s not valid here", ext_name);
					goto err;
				}
				if (*sk_providerp != NULL) {
					error_f("%s already set", ext_name);
					goto err;
				}
				if ((r = sshbuf_get_cstring(m,
				    sk_providerp, NULL)) != 0) {
					error_fr(r, "parse %s", ext_name);
					goto err;
				}
			} else {
				error_f("unsupported constraint \"%s\"",
				    ext_name);
				goto err;
			}
			free(ext_name);
			break;
		default:
			error_f("Unknown constraint %d", ctype);
 err:
			free(ext_name);
			sshbuf_free(b);
			return -1;
		}
	}
	/* success */
	return 0;
}