fmt_entry(TERMTYPE2 *tterm,
	  PredFunc pred,
	  int content_only,
	  int suppress_untranslatable,
	  int infodump,
	  int numbers)
{
    PredIdx i, j;
    char buffer[MAX_TERMINFO_LENGTH + EXTRA_CAP];
    char *capability;
    NCURSES_CONST char *name;
    int predval, len;
    PredIdx num_bools = 0;
    PredIdx num_values = 0;
    PredIdx num_strings = 0;
    bool outcount = 0;

#define WRAP_CONCAT1(s)		wrap_concat1(s); outcount = TRUE
#define WRAP_CONCAT		WRAP_CONCAT1(buffer)

    len = 12;			/* terminfo file-header */

    if (pred == 0) {
	cur_type = tterm;
	pred = dump_predicate;
    }

    strcpy_DYN(&outbuf, 0);
    if (content_only) {
	column = indent;	/* FIXME: workaround to prevent empty lines */
    } else {
	strcpy_DYN(&outbuf, tterm->term_names);

	/*
	 * Colon is legal in terminfo descriptions, but not in termcap.
	 */
	if (!infodump) {
	    char *p = outbuf.text;
	    while (*p) {
		if (*p == ':') {
		    *p = '=';
		}
		++p;
	    }
	}
	strcpy_DYN(&outbuf, separator);
	column = (int) outbuf.used;
	if (height > 1)
	    force_wrap();
    }

    for_each_boolean(j, tterm) {
	i = BoolIndirect(j);
	name = ExtBoolname(tterm, (int) i, bool_names);
	assert(strlen(name) < sizeof(buffer) - EXTRA_CAP);

	if (!version_filter(BOOLEAN, i))
	    continue;
	else if (isObsolete(outform, name))
	    continue;

	predval = pred(BOOLEAN, i);
	if (predval != FAIL) {
	    _nc_STRCPY(buffer, name, sizeof(buffer));
	    if (predval <= 0)
		_nc_STRCAT(buffer, "@", sizeof(buffer));
	    else if (i + 1 > num_bools)
		num_bools = i + 1;
	    WRAP_CONCAT;
	}
    }

    if (column != indent && height > 1)
	force_wrap();

    for_each_number(j, tterm) {
	i = NumIndirect(j);
	name = ExtNumname(tterm, (int) i, num_names);
	assert(strlen(name) < sizeof(buffer) - EXTRA_CAP);

	if (!version_filter(NUMBER, i))
	    continue;
	else if (isObsolete(outform, name))
	    continue;

	predval = pred(NUMBER, i);
	if (predval != FAIL) {
	    if (tterm->Numbers[i] < 0) {
		_nc_SPRINTF(buffer, _nc_SLIMIT(sizeof(buffer))
			    "%s@", name);
	    } else {
		size_t nn;
		_nc_SPRINTF(buffer, _nc_SLIMIT(sizeof(buffer))
			    "%s#", name);
		nn = strlen(buffer);
		_nc_SPRINTF(buffer + nn, _nc_SLIMIT(sizeof(buffer) - nn)
			    number_format(tterm->Numbers[i]),
			    tterm->Numbers[i]);
		if (i + 1 > num_values)
		    num_values = i + 1;
	    }
	    WRAP_CONCAT;
	}
    }

    if (column != indent && height > 1)
	force_wrap();

    len += (int) (num_bools
		  + num_values * 2
		  + strlen(tterm->term_names) + 1);
    if (len & 1)
	len++;

#undef CUR
#define CUR tterm->
    if (outform == F_TERMCAP) {
	if (VALID_STRING(termcap_reset)) {
	    if (VALID_STRING(init_3string)
		&& !strcmp(init_3string, termcap_reset))
		DISCARD(init_3string);

	    if (VALID_STRING(reset_2string)
		&& !strcmp(reset_2string, termcap_reset))
		DISCARD(reset_2string);
	}
    }

    for_each_string(j, tterm) {
	i = StrIndirect(j);
	name = ExtStrname(tterm, (int) i, str_names);
	assert(strlen(name) < sizeof(buffer) - EXTRA_CAP);

	capability = tterm->Strings[i];

	if (!version_filter(STRING, i))
	    continue;
	else if (isObsolete(outform, name))
	    continue;

#if NCURSES_XNAMES
	/*
	 * Extended names can be longer than 2 characters, but termcap programs
	 * cannot read those (filter them out).
	 */
	if (outform == F_TERMCAP && (strlen(name) > 2))
	    continue;
#endif

	if (outform == F_TERMCAP) {
	    /*
	     * Some older versions of vi want rmir/smir to be defined
	     * for ich/ich1 to work.  If they're not defined, force
	     * them to be output as defined and empty.
	     */
	    if (PRESENT(insert_character) || PRESENT(parm_ich)) {
		if (SAME_CAP(i, enter_insert_mode)
		    && enter_insert_mode == ABSENT_STRING) {
		    _nc_STRCPY(buffer, "im=", sizeof(buffer));
		    WRAP_CONCAT;
		    continue;
		}

		if (SAME_CAP(i, exit_insert_mode)
		    && exit_insert_mode == ABSENT_STRING) {
		    _nc_STRCPY(buffer, "ei=", sizeof(buffer));
		    WRAP_CONCAT;
		    continue;
		}
	    }
	    /*
	     * termcap applications such as screen will be confused if sgr0
	     * is translated to a string containing rmacs.  Filter that out.
	     */
	    if (PRESENT(exit_attribute_mode)) {
		if (SAME_CAP(i, exit_attribute_mode)) {
		    char *trimmed_sgr0;
		    char *my_sgr = set_attributes;

		    set_attributes = save_sgr;

		    trimmed_sgr0 = _nc_trim_sgr0(tterm);
		    if (strcmp(capability, trimmed_sgr0)) {
			capability = trimmed_sgr0;
		    } else {
			if (trimmed_sgr0 != exit_attribute_mode)
			    free(trimmed_sgr0);
		    }

		    set_attributes = my_sgr;
		}
	    }
	}

	predval = pred(STRING, i);
	buffer[0] = '\0';

	if (predval != FAIL) {
	    if (VALID_STRING(capability)
		&& i + 1 > num_strings)
		num_strings = i + 1;

	    if (!VALID_STRING(capability)) {
		_nc_SPRINTF(buffer, _nc_SLIMIT(sizeof(buffer))
			    "%s@", name);
		WRAP_CONCAT;
	    } else if (TcOutput()) {
		char *srccap = _nc_tic_expand(capability, TRUE, numbers);
		int params = ((i < (int) SIZEOF(parametrized))
			      ? parametrized[i]
			      : ((*srccap == 'k')
				 ? 0
				 : has_params(srccap)));
		char *cv = _nc_infotocap(name, srccap, params);

		if (cv == 0) {
		    if (outform == F_TCONVERR) {
			_nc_SPRINTF(buffer, _nc_SLIMIT(sizeof(buffer))
				    "%s=!!! %s WILL NOT CONVERT !!!",
				    name, srccap);
			WRAP_CONCAT;
		    } else if (suppress_untranslatable) {
			continue;
		    } else {
			char *s = srccap, *d = buffer;
			int need = 3 + (int) strlen(name);
			while ((*d = *s++) != 0) {
			    if ((d - buffer + 1) >= (int) sizeof(buffer)) {
				fprintf(stderr,
					"%s: value for %s is too long\n",
					_nc_progname,
					name);
				*d = '\0';
				break;
			    }
			    if (*d == ':') {
				*d++ = '\\';
				*d = ':';
			    } else if (*d == '\\') {
				*++d = *s++;
			    }
			    d++;
			    *d = '\0';
			}
			need += (int) (d - buffer);
			wrap_concat("..", need, w1ST | wERR);
			need -= 2;
			wrap_concat(name, need, wOFF | wERR);
			need -= (int) strlen(name);
			wrap_concat("=", need, w2ND | wERR);
			need -= 1;
			wrap_concat(buffer, need, wEND | wERR);
			outcount = TRUE;
		    }
		} else {
		    wrap_concat3(name, "=", cv);
		}
		len += (int) strlen(capability) + 1;
	    } else {
		char *src = _nc_tic_expand(capability,
					   outform == F_TERMINFO, numbers);

		strcpy_DYN(&tmpbuf, 0);
		strcpy_DYN(&tmpbuf, name);
		strcpy_DYN(&tmpbuf, "=");
		if (pretty
		    && (outform == F_TERMINFO
			|| outform == F_VARIABLE)) {
		    fmt_complex(tterm, name, src, 1);
		} else {
		    strcpy_DYN(&tmpbuf, src);
		}
		len += (int) strlen(capability) + 1;
		WRAP_CONCAT1(tmpbuf.text);
	    }
	}
	/* e.g., trimmed_sgr0 */
	if (VALID_STRING(capability) &&
	    capability != tterm->Strings[i])
	    free(capability);
    }
    len += (int) (num_strings * 2);

    /*
     * This piece of code should be an effective inverse of the functions
     * postprocess_terminfo() and postprocess_terminfo() in parse_entry.c.
     * Much more work should be done on this to support dumping termcaps.
     */
    if (tversion == V_HPUX) {
	if (VALID_STRING(memory_lock)) {
	    _nc_SPRINTF(buffer, _nc_SLIMIT(sizeof(buffer))
			"meml=%s", memory_lock);
	    WRAP_CONCAT;
	}
	if (VALID_STRING(memory_unlock)) {
	    _nc_SPRINTF(buffer, _nc_SLIMIT(sizeof(buffer))
			"memu=%s", memory_unlock);
	    WRAP_CONCAT;
	}
    } else if (tversion == V_AIX) {
	if (VALID_STRING(acs_chars)) {
	    bool box_ok = TRUE;
	    const char *acstrans = "lqkxjmwuvtn";
	    const char *cp;
	    char *tp, *sp, boxchars[11];

	    tp = boxchars;
	    for (cp = acstrans; *cp; cp++) {
		sp = (strchr) (acs_chars, *cp);
		if (sp)
		    *tp++ = sp[1];
		else {
		    box_ok = FALSE;
		    break;
		}
	    }
	    tp[0] = '\0';

	    if (box_ok) {
		char *tmp = _nc_tic_expand(boxchars,
					   (outform == F_TERMINFO),
					   numbers);
		_nc_STRCPY(buffer, "box1=", sizeof(buffer));
		while (*tmp != '\0') {
		    size_t have = strlen(buffer);
		    size_t next = strlen(tmp);
		    size_t want = have + next + 1;
		    size_t last = next;
		    char save = '\0';

		    /*
		     * If the expanded string is too long for the buffer,
		     * chop it off and save the location where we chopped it.
		     */
		    if (want >= sizeof(buffer)) {
			save = tmp[last];
			tmp[last] = '\0';
		    }
		    _nc_STRCAT(buffer, tmp, sizeof(buffer));

		    /*
		     * If we chopped the buffer, replace the missing piece and
		     * shift everything to append the remainder.
		     */
		    if (save != '\0') {
			next = 0;
			tmp[last] = save;
			while ((tmp[next] = tmp[last + next]) != '\0') {
			    ++next;
			}
		    } else {
			break;
		    }
		}
		WRAP_CONCAT;
	    }
	}
    }

    /*
     * kludge: trim off trailer to avoid an extra blank line
     * in infocmp -u output when there are no string differences
     */
    if (outcount) {
	bool trimmed = FALSE;
	j = (PredIdx) outbuf.used;
	if (wrapped && did_wrap) {
	    /* EMPTY */ ;
	} else if (j >= 2
		   && outbuf.text[j - 1] == '\t'
		   && outbuf.text[j - 2] == '\n') {
	    outbuf.used -= 2;
	    trimmed = TRUE;
	} else if (j >= 4
		   && outbuf.text[j - 1] == ':'
		   && outbuf.text[j - 2] == '\t'
		   && outbuf.text[j - 3] == '\n'
		   && outbuf.text[j - 4] == '\\') {
	    outbuf.used -= 4;
	    trimmed = TRUE;
	}
	if (trimmed) {
	    outbuf.text[outbuf.used] = '\0';
	    column = oldcol;
	    strcpy_DYN(&outbuf, " ");
	}
    }
#if 0
    fprintf(stderr, "num_bools = %d\n", num_bools);
    fprintf(stderr, "num_values = %d\n", num_values);
    fprintf(stderr, "num_strings = %d\n", num_strings);
    fprintf(stderr, "term_names=%s, len=%d, strlen(outbuf)=%d, outbuf=%s\n",
	    tterm->term_names, len, outbuf.used, outbuf.text);
#endif
    /*
     * Here's where we use infodump to trigger a more stringent length check
     * for termcap-translation purposes.
     * Return the length of the raw entry, without tc= expansions,
     * It gives an idea of which entries are deadly to even *scan past*,
     * as opposed to *use*.
     */
    return (infodump ? len : (int) termcap_length(outbuf.text));
}