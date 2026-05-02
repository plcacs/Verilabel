static Token *tokenize(char *line)
{
    char c, *p = line;
    enum pp_token_type type;
    Token *list = NULL;
    Token *t, **tail = &list;

    while (*line) {
        p = line;
        if (*p == '%') {
            p++;
            if (*p == '+' && !nasm_isdigit(p[1])) {
                p++;
                type = TOK_PASTE;
            } else if (nasm_isdigit(*p) ||
                       ((*p == '-' || *p == '+') && nasm_isdigit(p[1]))) {
                do {
                    p++;
                }
                while (nasm_isdigit(*p));
                type = TOK_PREPROC_ID;
            } else if (*p == '{') {
                p++;
                while (*p) {
                    if (*p == '}')
                        break;
                    p[-1] = *p;
                    p++;
                }
                if (*p != '}')
                    nasm_error(ERR_WARNING | ERR_PASS1,
			       "unterminated %%{ construct");
                p[-1] = '\0';
                if (*p)
                    p++;
                type = TOK_PREPROC_ID;
            } else if (*p == '[') {
                int lvl = 1;
                line += 2;      /* Skip the leading %[ */
                p++;
                while (lvl && (c = *p++)) {
                    switch (c) {
                    case ']':
                        lvl--;
                        break;
                    case '%':
                        if (*p == '[')
                            lvl++;
                        break;
                    case '\'':
                    case '\"':
                    case '`':
                        p = nasm_skip_string(p - 1) + 1;
                        break;
                    default:
                        break;
                    }
                }
                p--;
                if (*p)
                    *p++ = '\0';
                if (lvl)
                    nasm_error(ERR_NONFATAL|ERR_PASS1,
			       "unterminated %%[ construct");
                type = TOK_INDIRECT;
            } else if (*p == '?') {
                type = TOK_PREPROC_Q; /* %? */
                p++;
                if (*p == '?') {
                    type = TOK_PREPROC_QQ; /* %?? */
                    p++;
                }
            } else if (*p == '!') {
                type = TOK_PREPROC_ID;
                p++;
                if (isidchar(*p)) {
                    do {
                        p++;
                    }
                    while (isidchar(*p));
                } else if (*p == '\'' || *p == '\"' || *p == '`') {
                    p = nasm_skip_string(p);
                    if (*p)
                        p++;
                    else
                        nasm_error(ERR_NONFATAL|ERR_PASS1,
				   "unterminated %%! string");
                } else {
                    /* %! without string or identifier */
                    type = TOK_OTHER; /* Legacy behavior... */
                }
            } else if (isidchar(*p) ||
                       ((*p == '!' || *p == '%' || *p == '$') &&
                        isidchar(p[1]))) {
                do {
                    p++;
                }
                while (isidchar(*p));
                type = TOK_PREPROC_ID;
            } else {
                type = TOK_OTHER;
                if (*p == '%')
                    p++;
            }
        } else if (isidstart(*p) || (*p == '$' && isidstart(p[1]))) {
            type = TOK_ID;
            p++;
            while (*p && isidchar(*p))
                p++;
        } else if (*p == '\'' || *p == '"' || *p == '`') {
            /*
             * A string token.
             */
            type = TOK_STRING;
            p = nasm_skip_string(p);

            if (*p) {
                p++;
            } else {
                nasm_error(ERR_WARNING|ERR_PASS1, "unterminated string");
                /* Handling unterminated strings by UNV */
                /* type = -1; */
            }
        } else if (p[0] == '$' && p[1] == '$') {
            type = TOK_OTHER;   /* TOKEN_BASE */
            p += 2;
        } else if (isnumstart(*p)) {
            bool is_hex = false;
            bool is_float = false;
            bool has_e = false;
            char c, *r;

            /*
             * A numeric token.
             */

            if (*p == '$') {
                p++;
                is_hex = true;
            }

            for (;;) {
                c = *p++;

                if (!is_hex && (c == 'e' || c == 'E')) {
                    has_e = true;
                    if (*p == '+' || *p == '-') {
                        /*
                         * e can only be followed by +/- if it is either a
                         * prefixed hex number or a floating-point number
                         */
                        p++;
                        is_float = true;
                    }
                } else if (c == 'H' || c == 'h' || c == 'X' || c == 'x') {
                    is_hex = true;
                } else if (c == 'P' || c == 'p') {
                    is_float = true;
                    if (*p == '+' || *p == '-')
                        p++;
                } else if (isnumchar(c))
                    ; /* just advance */
                else if (c == '.') {
                    /*
                     * we need to deal with consequences of the legacy
                     * parser, like "1.nolist" being two tokens
                     * (TOK_NUMBER, TOK_ID) here; at least give it
                     * a shot for now.  In the future, we probably need
                     * a flex-based scanner with proper pattern matching
                     * to do it as well as it can be done.  Nothing in
                     * the world is going to help the person who wants
                     * 0x123.p16 interpreted as two tokens, though.
                     */
                    r = p;
                    while (*r == '_')
                        r++;

                    if (nasm_isdigit(*r) || (is_hex && nasm_isxdigit(*r)) ||
                        (!is_hex && (*r == 'e' || *r == 'E')) ||
                        (*r == 'p' || *r == 'P')) {
                        p = r;
                        is_float = true;
                    } else
                        break;  /* Terminate the token */
                } else
                    break;
            }
            p--;        /* Point to first character beyond number */

            if (p == line+1 && *line == '$') {
                type = TOK_OTHER; /* TOKEN_HERE */
            } else {
                if (has_e && !is_hex) {
                    /* 1e13 is floating-point, but 1e13h is not */
                    is_float = true;
                }

                type = is_float ? TOK_FLOAT : TOK_NUMBER;
            }
        } else if (nasm_isspace(*p)) {
            type = TOK_WHITESPACE;
            p = nasm_skip_spaces(p);
            /*
             * Whitespace just before end-of-line is discarded by
             * pretending it's a comment; whitespace just before a
             * comment gets lumped into the comment.
             */
            if (!*p || *p == ';') {
                type = TOK_COMMENT;
                while (*p)
                    p++;
            }
        } else if (*p == ';') {
            type = TOK_COMMENT;
            while (*p)
                p++;
        } else {
            /*
             * Anything else is an operator of some kind. We check
             * for all the double-character operators (>>, <<, //,
             * %%, <=, >=, ==, !=, <>, &&, ||, ^^), but anything
             * else is a single-character operator.
             */
            type = TOK_OTHER;
            if ((p[0] == '>' && p[1] == '>') ||
                (p[0] == '<' && p[1] == '<') ||
                (p[0] == '/' && p[1] == '/') ||
                (p[0] == '<' && p[1] == '=') ||
                (p[0] == '>' && p[1] == '=') ||
                (p[0] == '=' && p[1] == '=') ||
                (p[0] == '!' && p[1] == '=') ||
                (p[0] == '<' && p[1] == '>') ||
                (p[0] == '&' && p[1] == '&') ||
                (p[0] == '|' && p[1] == '|') ||
                (p[0] == '^' && p[1] == '^')) {
                p++;
            }
            p++;
        }

        /* Handling unterminated string by UNV */
        /*if (type == -1)
          {
          *tail = t = new_Token(NULL, TOK_STRING, line, p-line+1);
          t->text[p-line] = *line;
          tail = &t->next;
          }
          else */
        if (type != TOK_COMMENT) {
            *tail = t = new_Token(NULL, type, line, p - line);
            tail = &t->next;
        }
        line = p;
    }
    return list;
}