S_grok_bslash_N(pTHX_ RExC_state_t *pRExC_state,
                regnode ** node_p,
                UV * code_point_p,
                int * cp_count,
                I32 * flagp,
                const bool strict,
                const U32 depth
    )
{
 /* This routine teases apart the various meanings of \N and returns
  * accordingly.  The input parameters constrain which meaning(s) is/are valid
  * in the current context.
  *
  * Exactly one of <node_p> and <code_point_p> must be non-NULL.
  *
  * If <code_point_p> is not NULL, the context is expecting the result to be a
  * single code point.  If this \N instance turns out to a single code point,
  * the function returns TRUE and sets *code_point_p to that code point.
  *
  * If <node_p> is not NULL, the context is expecting the result to be one of
  * the things representable by a regnode.  If this \N instance turns out to be
  * one such, the function generates the regnode, returns TRUE and sets *node_p
  * to point to that regnode.
  *
  * If this instance of \N isn't legal in any context, this function will
  * generate a fatal error and not return.
  *
  * On input, RExC_parse should point to the first char following the \N at the
  * time of the call.  On successful return, RExC_parse will have been updated
  * to point to just after the sequence identified by this routine.  Also
  * *flagp has been updated as needed.
  *
  * When there is some problem with the current context and this \N instance,
  * the function returns FALSE, without advancing RExC_parse, nor setting
  * *node_p, nor *code_point_p, nor *flagp.
  *
  * If <cp_count> is not NULL, the caller wants to know the length (in code
  * points) that this \N sequence matches.  This is set even if the function
  * returns FALSE, as detailed below.
  *
  * There are 5 possibilities here, as detailed in the next 5 paragraphs.
  *
  * Probably the most common case is for the \N to specify a single code point.
  * *cp_count will be set to 1, and *code_point_p will be set to that code
  * point.
  *
  * Another possibility is for the input to be an empty \N{}, which for
  * backwards compatibility we accept.  *cp_count will be set to 0. *node_p
  * will be set to a generated NOTHING node.
  *
  * Still another possibility is for the \N to mean [^\n]. *cp_count will be
  * set to 0. *node_p will be set to a generated REG_ANY node.
  *
  * The fourth possibility is that \N resolves to a sequence of more than one
  * code points.  *cp_count will be set to the number of code points in the
  * sequence. *node_p * will be set to a generated node returned by this
  * function calling S_reg().
  *
  * The final possibility is that it is premature to be calling this function;
  * that pass1 needs to be restarted.  This can happen when this changes from
  * /d to /u rules, or when the pattern needs to be upgraded to UTF-8.  The
  * latter occurs only when the fourth possibility would otherwise be in
  * effect, and is because one of those code points requires the pattern to be
  * recompiled as UTF-8.  The function returns FALSE, and sets the
  * RESTART_PASS1 and NEED_UTF8 flags in *flagp, as appropriate.  When this
  * happens, the caller needs to desist from continuing parsing, and return
  * this information to its caller.  This is not set for when there is only one
  * code point, as this can be called as part of an ANYOF node, and they can
  * store above-Latin1 code points without the pattern having to be in UTF-8.
  *
  * For non-single-quoted regexes, the tokenizer has resolved character and
  * sequence names inside \N{...} into their Unicode values, normalizing the
  * result into what we should see here: '\N{U+c1.c2...}', where c1... are the
  * hex-represented code points in the sequence.  This is done there because
  * the names can vary based on what charnames pragma is in scope at the time,
  * so we need a way to take a snapshot of what they resolve to at the time of
  * the original parse. [perl #56444].
  *
  * That parsing is skipped for single-quoted regexes, so we may here get
  * '\N{NAME}'.  This is a fatal error.  These names have to be resolved by the
  * parser.  But if the single-quoted regex is something like '\N{U+41}', that
  * is legal and handled here.  The code point is Unicode, and has to be
  * translated into the native character set for non-ASCII platforms.
  */

    char * endbrace;    /* points to '}' following the name */
    char *endchar;	/* Points to '.' or '}' ending cur char in the input
                           stream */
    char* p = RExC_parse; /* Temporary */

    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_GROK_BSLASH_N;

    GET_RE_DEBUG_FLAGS;

    assert(cBOOL(node_p) ^ cBOOL(code_point_p));  /* Exactly one should be set */
    assert(! (node_p && cp_count));               /* At most 1 should be set */

    if (cp_count) {     /* Initialize return for the most common case */
        *cp_count = 1;
    }

    /* The [^\n] meaning of \N ignores spaces and comments under the /x
     * modifier.  The other meanings do not, so use a temporary until we find
     * out which we are being called with */
    skip_to_be_ignored_text(pRExC_state, &p,
                            FALSE /* Don't force to /x */ );

    /* Disambiguate between \N meaning a named character versus \N meaning
     * [^\n].  The latter is assumed when the {...} following the \N is a legal
     * quantifier, or there is no '{' at all */
    if (*p != '{' || regcurly(p)) {
	RExC_parse = p;
        if (cp_count) {
            *cp_count = -1;
        }

	if (! node_p) {
            return FALSE;
        }

	*node_p = reg_node(pRExC_state, REG_ANY);
	*flagp |= HASWIDTH|SIMPLE;
	MARK_NAUGHTY(1);
        Set_Node_Length(*node_p, 1); /* MJD */
	return TRUE;
    }

    /* Here, we have decided it should be a named character or sequence */

    /* The test above made sure that the next real character is a '{', but
     * under the /x modifier, it could be separated by space (or a comment and
     * \n) and this is not allowed (for consistency with \x{...} and the
     * tokenizer handling of \N{NAME}). */
    if (*RExC_parse != '{') {
	vFAIL("Missing braces on \\N{}");
    }

    RExC_parse++;	/* Skip past the '{' */

    endbrace = (char *) memchr(RExC_parse, '}', RExC_end - RExC_parse);
    if (! endbrace) { /* no trailing brace */
        vFAIL2("Missing right brace on \\%c{}", 'N');
    }
    else if (!(   endbrace == RExC_parse	/* nothing between the {} */
               || memBEGINs(RExC_parse,   /* U+ (bad hex is checked below
                                                   for a  better error msg) */
                                  (STRLEN) (RExC_end - RExC_parse),
                                 "U+")))
    {
	RExC_parse = endbrace;	/* position msg's '<--HERE' */
	vFAIL("\\N{NAME} must be resolved by the lexer");
    }

    REQUIRE_UNI_RULES(flagp, FALSE); /* Unicode named chars imply Unicode
                                        semantics */

    if (endbrace == RExC_parse) {   /* empty: \N{} */
        if (strict) {
            RExC_parse++;   /* Position after the "}" */
            vFAIL("Zero length \\N{}");
        }
        if (cp_count) {
            *cp_count = 0;
        }
        nextchar(pRExC_state);
	if (! node_p) {
            return FALSE;
        }

        *node_p = reg_node(pRExC_state,NOTHING);
        return TRUE;
    }

    RExC_parse += 2;	/* Skip past the 'U+' */

    /* Because toke.c has generated a special construct for us guaranteed not
     * to have NULs, we can use a str function */
    endchar = RExC_parse + strcspn(RExC_parse, ".}");

    /* Code points are separated by dots.  If none, there is only one code
     * point, and is terminated by the brace */

    if (endchar >= endbrace) {
	STRLEN length_of_hex;
	I32 grok_hex_flags;

        /* Here, exactly one code point.  If that isn't what is wanted, fail */
        if (! code_point_p) {
            RExC_parse = p;
            return FALSE;
        }

        /* Convert code point from hex */
	length_of_hex = (STRLEN)(endchar - RExC_parse);
	grok_hex_flags = PERL_SCAN_ALLOW_UNDERSCORES
                       | PERL_SCAN_DISALLOW_PREFIX

                           /* No errors in the first pass (See [perl
                            * #122671].)  We let the code below find the
                            * errors when there are multiple chars. */
                       | ((SIZE_ONLY)
                          ? PERL_SCAN_SILENT_ILLDIGIT
                          : 0);

        /* This routine is the one place where both single- and double-quotish
         * \N{U+xxxx} are evaluated.  The value is a Unicode code point which
         * must be converted to native. */
	*code_point_p = UNI_TO_NATIVE(grok_hex(RExC_parse,
                                               &length_of_hex,
                                               &grok_hex_flags,
                                               NULL));

	/* The tokenizer should have guaranteed validity, but it's possible to
         * bypass it by using single quoting, so check.  Don't do the check
         * here when there are multiple chars; we do it below anyway. */
        if (length_of_hex == 0
            || length_of_hex != (STRLEN)(endchar - RExC_parse) )
        {
            RExC_parse += length_of_hex;	/* Includes all the valid */
            RExC_parse += (RExC_orig_utf8)	/* point to after 1st invalid */
                            ? UTF8SKIP(RExC_parse)
                            : 1;
            /* Guard against malformed utf8 */
            if (RExC_parse >= endchar) {
                RExC_parse = endchar;
            }
            vFAIL("Invalid hexadecimal number in \\N{U+...}");
        }

        RExC_parse = endbrace + 1;
        return TRUE;
    }
    else {  /* Is a multiple character sequence */
	SV * substitute_parse;
	STRLEN len;
	char *orig_end = RExC_end;
	char *save_start = RExC_start;
        I32 flags;

        /* Count the code points, if desired, in the sequence */
        if (cp_count) {
            *cp_count = 0;
            while (RExC_parse < endbrace) {
                /* Point to the beginning of the next character in the sequence. */
                RExC_parse = endchar + 1;
                endchar = RExC_parse + strcspn(RExC_parse, ".}");
                (*cp_count)++;
            }
        }

        /* Fail if caller doesn't want to handle a multi-code-point sequence.
         * But don't backup up the pointer if the caller wants to know how many
         * code points there are (they can then handle things) */
        if (! node_p) {
            if (! cp_count) {
                RExC_parse = p;
            }
            return FALSE;
        }

	/* What is done here is to convert this to a sub-pattern of the form
         * \x{char1}\x{char2}...  and then call reg recursively to parse it
         * (enclosing in "(?: ... )" ).  That way, it retains its atomicness,
         * while not having to worry about special handling that some code
         * points may have. */

	substitute_parse = newSVpvs("?:");

	while (RExC_parse < endbrace) {

	    /* Convert to notation the rest of the code understands */
	    sv_catpv(substitute_parse, "\\x{");
	    sv_catpvn(substitute_parse, RExC_parse, endchar - RExC_parse);
	    sv_catpv(substitute_parse, "}");

	    /* Point to the beginning of the next character in the sequence. */
	    RExC_parse = endchar + 1;
	    endchar = RExC_parse + strcspn(RExC_parse, ".}");

	}
        sv_catpv(substitute_parse, ")");

        len = SvCUR(substitute_parse);

	/* Don't allow empty number */
	if (len < (STRLEN) 8) {
            RExC_parse = endbrace;
	    vFAIL("Invalid hexadecimal number in \\N{U+...}");
	}

        RExC_parse = RExC_start = RExC_adjusted_start
                                              = SvPV_nolen(substitute_parse);
	RExC_end = RExC_parse + len;

        /* The values are Unicode, and therefore not subject to recoding, but
         * have to be converted to native on a non-Unicode (meaning non-ASCII)
         * platform. */
#ifdef EBCDIC
        RExC_recode_x_to_native = 1;
#endif

        *node_p = reg(pRExC_state, 1, &flags, depth+1);

        /* Restore the saved values */
	RExC_start = RExC_adjusted_start = save_start;
	RExC_parse = endbrace;
	RExC_end = orig_end;
#ifdef EBCDIC
        RExC_recode_x_to_native = 0;
#endif
        SvREFCNT_dec_NN(substitute_parse);

        if (! *node_p) {
            if (flags & (RESTART_PASS1|NEED_UTF8)) {
                *flagp = flags & (RESTART_PASS1|NEED_UTF8);
                return FALSE;
            }
            FAIL2("panic: reg returned NULL to grok_bslash_N, flags=%#" UVxf,
                (UV) flags);
        }
        *flagp |= flags&(HASWIDTH|SPSTART|SIMPLE|POSTPONED);

        nextchar(pRExC_state);

        return TRUE;
    }
}