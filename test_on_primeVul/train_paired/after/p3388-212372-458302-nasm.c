static int parse_eops(extop **result, bool critical, int elem)
{
    extop *eop = NULL, *prev = NULL;
    extop **tail = result;
    int sign;
    int i = tokval.t_type;
    int oper_num = 0;
    bool do_subexpr = false;

    *tail = NULL;

    /* End of string is obvious; ) ends a sub-expression list e.g. DUP */
    for (i = tokval.t_type; i != TOKEN_EOS; i = stdscan(NULL, &tokval)) {
        char endparen = ')';   /* Is a right paren the end of list? */

        if (i == ')')
            break;

        if (!eop) {
            nasm_new(eop);
            eop->dup  = 1;
            eop->elem = elem;
            do_subexpr = false;
        }
        sign = +1;

        /*
         * end_expression_next() here is to distinguish this from
         * a string used as part of an expression...
         */
        if (i == TOKEN_QMARK) {
            eop->type = EOT_DB_RESERVE;
        } else if (do_subexpr && i == '(') {
            extop *subexpr;

            stdscan(NULL, &tokval); /* Skip paren */
            if (parse_eops(&eop->val.subexpr, critical, eop->elem) < 0)
                goto fail;

            subexpr = eop->val.subexpr;
            if (!subexpr) {
                /* Subexpression is empty */
                eop->type = EOT_NOTHING;
            } else if (!subexpr->next) {
                /*
                 * Subexpression is a single element, flatten.
                 * Note that if subexpr has an allocated buffer associated
                 * with it, freeing it would free the buffer, too, so
                 * we need to move subexpr up, not eop down.
                 */
                if (!subexpr->elem)
                    subexpr->elem = eop->elem;
                subexpr->dup *= eop->dup;
                nasm_free(eop);
                eop = subexpr;
            } else {
                eop->type = EOT_EXTOP;
            }

            /* We should have ended on a closing paren */
            if (tokval.t_type != ')') {
                nasm_nonfatal("expected `)' after subexpression, got `%s'",
                              i == TOKEN_EOS ?
                              "end of line" : tokval.t_charptr);
                goto fail;
            }
            endparen = 0;       /* This time the paren is not the end */
        } else if (i == '%') {
            /* %(expression_list) */
            do_subexpr = true;
            continue;
        } else if (i == TOKEN_SIZE) {
            /* Element size override */
            eop->elem = tokval.t_inttwo;
            do_subexpr = true;
            continue;
        } else if (i == TOKEN_STR && end_expression_next()) {
            eop->type            = EOT_DB_STRING;
            eop->val.string.data = tokval.t_charptr;
            eop->val.string.len  = tokval.t_inttwo;
        } else if (i == TOKEN_STRFUNC) {
            bool parens = false;
            const char *funcname = tokval.t_charptr;
            enum strfunc func = tokval.t_integer;

            i = stdscan(NULL, &tokval);
            if (i == '(') {
                parens = true;
                endparen = 0;
                i = stdscan(NULL, &tokval);
            }
            if (i != TOKEN_STR) {
                nasm_nonfatal("%s must be followed by a string constant",
                              funcname);
                eop->type = EOT_NOTHING;
            } else {
                eop->type = EOT_DB_STRING_FREE;
                eop->val.string.len =
                    string_transform(tokval.t_charptr, tokval.t_inttwo,
                                     &eop->val.string.data, func);
                if (eop->val.string.len == (size_t)-1) {
                    nasm_nonfatal("invalid input string to %s", funcname);
                    eop->type = EOT_NOTHING;
                }
            }
            if (parens && i && i != ')') {
                i = stdscan(NULL, &tokval);
                if (i != ')')
                    nasm_nonfatal("unterminated %s function", funcname);
            }
        } else if (i == '-' || i == '+') {
            char *save = stdscan_get();
            struct tokenval tmptok;

            sign = (i == '-') ? -1 : 1;
            if (stdscan(NULL, &tmptok) != TOKEN_FLOAT) {
                stdscan_set(save);
                goto is_expression;
            } else {
                tokval = tmptok;
                goto is_float;
            }
        } else if (i == TOKEN_FLOAT) {
            enum floatize fmt;
        is_float:
            eop->type = EOT_DB_FLOAT;

            fmt = float_deffmt(eop->elem);
            if (fmt == FLOAT_ERR) {
                nasm_nonfatal("no %d-bit floating-point format supported",
                              eop->elem << 3);
                eop->val.string.len = 0;
            } else if (eop->elem < 1) {
                nasm_nonfatal("floating-point constant"
                              " encountered in unknown instruction");
                /*
                 * fix suggested by Pedro Gimeno... original line was:
                 * eop->type = EOT_NOTHING;
                 */
                eop->val.string.len = 0;
            } else {
                eop->val.string.len = eop->elem;

                eop = nasm_realloc(eop, sizeof(extop) + eop->val.string.len);
                eop->val.string.data = (char *)eop + sizeof(extop);
                if (!float_const(tokval.t_charptr, sign,
                                 (uint8_t *)eop->val.string.data, fmt))
                    eop->val.string.len = 0;
            }
            if (!eop->val.string.len)
                eop->type = EOT_NOTHING;
        } else {
            /* anything else, assume it is an expression */
            expr *value;

        is_expression:
            value = evaluate(stdscan, NULL, &tokval, NULL,
                             critical, NULL);
            i = tokval.t_type;
            if (!value)                  /* Error in evaluator */
                goto fail;
            if (tokval.t_flag & TFLAG_DUP) {
                /* Expression followed by DUP */
                if (!is_simple(value)) {
                    nasm_nonfatal("non-constant argument supplied to DUP");
                    goto fail;
                } else if (value->value < 0) {
                    nasm_nonfatal("negative argument supplied to DUP");
                    goto fail;
                }
                eop->dup *= (size_t)value->value;
                do_subexpr = true;
                continue;
            }
            if (value_to_extop(value, eop, location.segment)) {
                nasm_nonfatal("expression is not simple or relocatable");
            }
        }

        if (eop->dup == 0 || eop->type == EOT_NOTHING) {
            nasm_free(eop);
        } else if (eop->type == EOT_DB_RESERVE &&
                   prev && prev->type == EOT_DB_RESERVE &&
                   prev->elem == eop->elem) {
            /* Coalesce multiple EOT_DB_RESERVE */
            prev->dup += eop->dup;
            nasm_free(eop);
        } else {
            /* Add this eop to the end of the chain */
            prev = eop;
            *tail = eop;
            tail = &eop->next;
        }

        oper_num++;
        eop = NULL;             /* Done with this operand */

        /*
         * We're about to call stdscan(), which will eat the
         * comma that we're currently sitting on between
         * arguments. However, we'd better check first that it
         * _is_ a comma.
         */
        if (i == TOKEN_EOS || i == endparen)	/* Already at end? */
            break;
        if (i != ',') {
            i = stdscan(NULL, &tokval);		/* eat the comma or final paren */
            if (i == TOKEN_EOS || i == ')')	/* got end of expression */
                break;
            if (i != ',') {
                nasm_nonfatal("comma expected after operand");
                goto fail;
            }
        }
    }

    return oper_num;

fail:
    if (eop)
        nasm_free(eop);
    return -1;
}