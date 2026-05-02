enhanced_recursion(
    const char *p,
    TBOOLEAN brace,
    char *fontname,
    double fontsize,
    double base,
    TBOOLEAN widthflag,
    TBOOLEAN showflag,
    int overprint)
{
    TBOOLEAN wasitalic, wasbold;

    /* Keep track of the style of the font passed in at this recursion level */
    wasitalic = (strstr(fontname, ":Italic") != NULL);
    wasbold = (strstr(fontname, ":Bold") != NULL);

    FPRINTF((stderr, "RECURSE WITH \"%s\", %d %s %.1f %.1f %d %d %d",
		p, brace, fontname, fontsize, base, widthflag, showflag, overprint));

    /* Start each recursion with a clean string */
    (term->enhanced_flush)();

    if (base + fontsize > enhanced_max_height) {
	enhanced_max_height = base + fontsize;
	ENH_DEBUG(("Setting max height to %.1f\n", enhanced_max_height));
    }

    if (base < enhanced_min_height) {
	enhanced_min_height = base;
	ENH_DEBUG(("Setting min height to %.1f\n", enhanced_min_height));
    }

    while (*p) {
	double shift;

	/*
	 * EAM Jun 2009 - treating bytes one at a time does not work for multibyte
	 * encodings, including utf-8. If we hit a byte with the high bit set, test
	 * whether it starts a legal UTF-8 sequence and if so copy the whole thing.
	 * Other multibyte encodings are still a problem.
	 * Gnuplot's other defined encodings are all single-byte; for those we
	 * really do want to treat one byte at a time.
	 */
	if ((*p & 0x80) && (encoding == S_ENC_DEFAULT || encoding == S_ENC_UTF8)) {
	    unsigned long utf8char;
	    const char *nextchar = p;

	    (term->enhanced_open)(fontname, fontsize, base, widthflag, showflag, overprint);
	    if (utf8toulong(&utf8char, &nextchar)) {	/* Legal UTF8 sequence */
		while (p < nextchar)
		    (term->enhanced_writec)(*p++);
		p--;
	    } else {					/* Some other multibyte encoding? */
		(term->enhanced_writec)(*p);
	    }
/* shige : for Shift_JIS */
	} else if ((*p & 0x80) && (encoding == S_ENC_SJIS)) {
	    (term->enhanced_open)(fontname, fontsize, base, widthflag, showflag, overprint);
	    (term->enhanced_writec)(*(p++));
	    (term->enhanced_writec)(*p);
	} else

	switch (*p) {
	case '}'  :
	    /*{{{  deal with it*/
	    if (brace)
		return (p);

	    int_warn(NO_CARET, "enhanced text parser - spurious }");
	    break;
	    /*}}}*/

	case '_'  :
	case '^'  :
	    /*{{{  deal with super/sub script*/
	    shift = (*p == '^') ? 0.5 : -0.3;
	    (term->enhanced_flush)();
	    p = enhanced_recursion(p + 1, FALSE, fontname, fontsize * 0.8,
			      base + shift * fontsize, widthflag,
			      showflag, overprint);
	    break;
	    /*}}}*/
	case '{'  :
	    {
		TBOOLEAN isitalic = FALSE, isbold = FALSE, isnormal = FALSE;
		const char *start_of_fontname = NULL;
		const char *end_of_fontname = NULL;
		char *localfontname = NULL;
		char ch;
		double f = fontsize, ovp;

		/* Mar 2014 - this will hold "fontfamily{:Italic}{:Bold}" */
		char *styledfontname = NULL;

		/*{{{  recurse (possibly with a new font) */

		ENH_DEBUG(("Dealing with {\n"));

		/* 30 Sep 2016:  Remove incorrect whitespace-eating loop going */
		/* waaay back to 31-May-2000 */        /* while (*++p == ' '); */
		++p;
		/* get vertical offset (if present) for overprinted text */
		if (overprint == 2) {
		    char *end;
		    ovp = strtod(p,&end);
		    p = end;
		    if (term->flags & TERM_IS_POSTSCRIPT)
			base = ovp*f;
		    else
			base += ovp*f;
		}
		--p;

		if (*++p == '/') {
		    /* then parse a fontname, optional fontsize */
		    while (*++p == ' ')
			;       /* do nothing */
		    if (*p=='-') {
			while (*++p == ' ')
			    ;   /* do nothing */
		    }
		    start_of_fontname = p;

		    /* Allow font name to be in quotes.
		     * This makes it possible to handle font names containing spaces.
		     */
		    if (*p == '\'' || *p == '"') {
			++p;
			while (*p != '\0' && *p != '}' && *p != *start_of_fontname)
			    ++p;
			if (*p != *start_of_fontname) {
			    int_warn(NO_CARET, "cannot interpret font name %s", start_of_fontname);
			    p = start_of_fontname;
			}
			start_of_fontname++;
			end_of_fontname = p++;
			ch = *p;
		    } else {

		    /* Normal unquoted font name */
			while ((ch = *p) > ' ' && ch != '=' && ch != '*' && ch != '}' && ch != ':')
			    ++p;
			end_of_fontname = p;
		    }

		    do {
			if (ch == '=') {
			    /* get optional font size */
			    char *end;
			    p++;
			    ENH_DEBUG(("Calling strtod(\"%s\") ...", p));
			    f = strtod(p, &end);
			    p = end;
			    ENH_DEBUG(("Returned %.1f and \"%s\"\n", f, p));

			    if (f == 0)
				f = fontsize;
			    else
				f *= enhanced_fontscale;  /* remember the scaling */

			    ENH_DEBUG(("Font size %.1f\n", f));
			} else if (ch == '*') {
			    /* get optional font size scale factor */
			    char *end;
			    p++;
			    ENH_DEBUG(("Calling strtod(\"%s\") ...", p));
			    f = strtod(p, &end);
			    p = end;
			    ENH_DEBUG(("Returned %.1f and \"%s\"\n", f, p));

			    if (f)
				f *= fontsize;  /* apply the scale factor */
			    else
				f = fontsize;

			    ENH_DEBUG(("Font size %.1f\n", f));
			} else if (ch == ':') {
			    /* get optional style markup attributes */
			    p++;
			    if (!strncmp(p,"Bold",4))
				isbold = TRUE;
			    if (!strncmp(p,"Italic",6))
				isitalic = TRUE;
			    if (!strncmp(p,"Normal",6))
				isnormal = TRUE;
			    while (isalpha((unsigned char)*p)) {p++;}
			}
		    } while (((ch = *p) == '=') || (ch == ':') || (ch == '*'));

		    if (ch == '}')
			int_warn(NO_CARET,"bad syntax in enhanced text string");

		    if (*p == ' ')	/* Eat up a single space following a font spec */
			++p;
		    if (!start_of_fontname || (start_of_fontname == end_of_fontname)) {
			/* Use the font name passed in to us */
			localfontname = gp_strdup(fontname);
		    } else {
			/* We found a new font name {/Font ...} */
			int len = end_of_fontname - start_of_fontname;
			localfontname = gp_alloc(len+1,"localfontname");
			strncpy(localfontname, start_of_fontname, len);
			localfontname[len] = '\0';
		    }
		}
		/*}}}*/

		/* Collect cumulative style markup before passing it in the font name */
		isitalic = (wasitalic || isitalic) && !isnormal;
		isbold = (wasbold || isbold) && !isnormal;

		styledfontname = stylefont(localfontname ? localfontname : fontname,
					    isbold, isitalic);

		p = enhanced_recursion(p, TRUE, styledfontname, f, base,
				  widthflag, showflag, overprint);

		(term->enhanced_flush)();

		free(styledfontname);
		free(localfontname);

		break;
	    } /* case '{' */
	case '@' :
	    /*{{{  phantom box - prints next 'char', then restores currentpoint */
	    (term->enhanced_flush)();
	    (term->enhanced_open)(fontname, fontsize, base, widthflag, showflag, 3);
	    p = enhanced_recursion(++p, FALSE, fontname, fontsize, base,
			      widthflag, showflag, overprint);
	    (term->enhanced_open)(fontname, fontsize, base, widthflag, showflag, 4);
	    break;
	    /*}}}*/

	case '&' :
	    /*{{{  character skip - skips space equal to length of character(s) */
	    (term->enhanced_flush)();

	    p = enhanced_recursion(++p, FALSE, fontname, fontsize, base,
			      widthflag, FALSE, overprint);
	    break;
	    /*}}}*/

	case '~' :
	    /*{{{ overprinted text */
	    /* the second string is overwritten on the first, centered
	     * horizontally on the first and (optionally) vertically
	     * shifted by an amount specified (as a fraction of the
	     * current fontsize) at the beginning of the second string

	     * Note that in this implementation neither the under- nor
	     * overprinted string can contain syntax that would result
	     * in additional recursions -- no subscripts,
	     * superscripts, or anything else, with the exception of a
	     * font definition at the beginning of the text */

	    (term->enhanced_flush)();
	    p = enhanced_recursion(++p, FALSE, fontname, fontsize, base,
			      widthflag, showflag, 1);
	    (term->enhanced_flush)();
	    if (!*p)
	        break;
	    p = enhanced_recursion(++p, FALSE, fontname, fontsize, base,
			      FALSE, showflag, 2);

	    overprint = 0;   /* may not be necessary, but just in case . . . */
	    break;
	    /*}}}*/

	case '('  :
	case ')'  :
	    /*{{{  an escape and print it */
	    /* special cases */
	    (term->enhanced_open)(fontname, fontsize, base, widthflag, showflag, overprint);
	    if (term->flags & TERM_IS_POSTSCRIPT)
		(term->enhanced_writec)('\\');
	    (term->enhanced_writec)(*p);
	    break;
	    /*}}}*/

	case '\\'  :
	    /*{{{  various types of escape sequences, some context-dependent */
	    (term->enhanced_open)(fontname, fontsize, base, widthflag, showflag, overprint);

	    /*     Unicode represented as \U+hhhhh where hhhhh is hexadecimal code point.
	     *     For UTF-8 encoding we translate hhhhh to a UTF-8 byte sequence and
	     *     output the bytes one by one.
	     */
	    if (p[1] == 'U' && p[2] == '+') {
		if (encoding == S_ENC_UTF8) {
		    uint32_t codepoint;
		    unsigned char utf8char[8];
		    int i, length;

		    sscanf(&(p[3]), "%5x", &codepoint);
		    length = ucs4toutf8(codepoint, utf8char);
		    p += (codepoint > 0xFFFF) ? 7 : 6;
		    for (i=0; i<length; i++)
			(term->enhanced_writec)(utf8char[i]);
		    break;
		}

	    /*     FIXME: non-utf8 environments not yet supported.
	     *     Note that some terminals may have an alternative way to handle unicode
	     *     escape sequences that is not dependent on encoding.
	     *     E.g. svg and html output could convert to xml sequences &#xhhhh;
	     *     For these cases we must retain the leading backslash so that the
	     *     unicode escape sequence can be recognized by the terminal driver.
	     */
		(term->enhanced_writec)(p[0]);
		break;
	    }

	    /* Enhanced mode always uses \xyz as an octal character representation
	     * but each terminal type must give us the actual output format wanted.
	     * pdf.trm wants the raw character code, which is why we use strtol();
	     * most other terminal types want some variant of "\\%o".
	     */
	    if (p[1] >= '0' && p[1] <= '7') {
		char *e, escape[16], octal[4] = {'\0','\0','\0','\0'};

		octal[0] = *(++p);
		if (p[1] >= '0' && p[1] <= '7') {
		    octal[1] = *(++p);
		    if (p[1] >= '0' && p[1] <= '7')
			octal[2] = *(++p);
		}
		sprintf(escape, enhanced_escape_format, strtol(octal,NULL,8));
		for (e=escape; *e; e++) {
		    (term->enhanced_writec)(*e);
		}
		break;
	    }

	    /* This was the original (prior to version 4) enhanced text code specific
	     * to the reserved characters of PostScript.
	     */
	    if (term->flags & TERM_IS_POSTSCRIPT) {
		if (p[1]=='\\' || p[1]=='(' || p[1]==')') {
		    (term->enhanced_writec)('\\');
		} else if (strchr("^_@&~{}",p[1]) == NULL) {
		    (term->enhanced_writec)('\\');
		    (term->enhanced_writec)('\\');
		    break;
		}
	    }

	    /* Step past the backslash character in the input stream */
	    ++p;

	    /* HBB: Avoid broken output if there's a \ exactly at the end of the line */
	    if (*p == '\0') {
		int_warn(NO_CARET, "enhanced text parser -- spurious backslash");
		break;
	    }

	    /* SVG requires an escaped '&' to be passed as something else */
	    /* FIXME: terminal-dependent code does not belong here */
	    if (*p == '&' && encoding == S_ENC_DEFAULT && !strcmp(term->name, "svg")) {
		(term->enhanced_writec)('\376');
		break;
	    }

	    /* print the character following the backslash */
	    (term->enhanced_writec)(*p);
	    break;
	    /*}}}*/

	default:
	    /*{{{  print it */
	    (term->enhanced_open)(fontname, fontsize, base, widthflag, showflag, overprint);
	    (term->enhanced_writec)(*p);
	    /*}}}*/
	} /* switch (*p) */

	/* like TeX, we only do one character in a recursion, unless it's
	 * in braces
	 */

	if (!brace) {
	    (term->enhanced_flush)();
	    return(p);  /* the ++p in the outer copy will increment us */
	}

	if (*p) /* only not true if { not terminated, I think */
	    ++p;
    } /* while (*p) */

    (term->enhanced_flush)();
    return p;
}