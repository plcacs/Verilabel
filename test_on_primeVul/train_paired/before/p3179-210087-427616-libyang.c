make_canonical(struct ly_ctx *ctx, int type, const char **value, void *data1, void *data2)
{
    const uint16_t buf_len = 511;
    char buf[buf_len + 1];
    struct lys_type_bit **bits = NULL;
    struct lyxp_expr *exp;
    const char *module_name, *cur_expr, *end;
    int i, j, count;
    int64_t num;
    uint64_t unum;
    uint8_t c;

#define LOGBUF(str) LOGERR(ctx, LY_EINVAL, "Value \"%s\" is too long.", str)

    switch (type) {
    case LY_TYPE_BITS:
        bits = (struct lys_type_bit **)data1;
        count = *((int *)data2);
        /* in canonical form, the bits are ordered by their position */
        buf[0] = '\0';
        for (i = 0; i < count; i++) {
            if (!bits[i]) {
                /* bit not set */
                continue;
            }
            if (buf[0]) {
                LY_CHECK_ERR_RETURN(strlen(buf) + 1 + strlen(bits[i]->name) > buf_len, LOGBUF(bits[i]->name), -1);
                sprintf(buf + strlen(buf), " %s", bits[i]->name);
            } else {
                LY_CHECK_ERR_RETURN(strlen(bits[i]->name) > buf_len, LOGBUF(bits[i]->name), -1);
                strcpy(buf, bits[i]->name);
            }
        }
        break;

    case LY_TYPE_IDENT:
        module_name = (const char *)data1;
        /* identity must always have a prefix */
        if (!strchr(*value, ':')) {
            sprintf(buf, "%s:%s", module_name, *value);
        } else {
            strcpy(buf, *value);
        }
        break;

    case LY_TYPE_INST:
        exp = lyxp_parse_expr(ctx, *value);
        LY_CHECK_ERR_RETURN(!exp, LOGINT(ctx), -1);

        module_name = NULL;
        count = 0;
        for (i = 0; (unsigned)i < exp->used; ++i) {
            cur_expr = &exp->expr[exp->expr_pos[i]];

            /* copy WS */
            if (i && ((end = exp->expr + exp->expr_pos[i - 1] + exp->tok_len[i - 1]) != cur_expr)) {
                if (count + (cur_expr - end) > buf_len) {
                    lyxp_expr_free(exp);
                    LOGBUF(end);
                    return -1;
                }
                strncpy(&buf[count], end, cur_expr - end);
                count += cur_expr - end;
            }

            if ((exp->tokens[i] == LYXP_TOKEN_NAMETEST) && (end = strnchr(cur_expr, ':', exp->tok_len[i]))) {
                /* get the module name with ":" */
                ++end;
                j = end - cur_expr;

                if (!module_name || strncmp(cur_expr, module_name, j)) {
                    /* print module name with colon, it does not equal to the parent one */
                    if (count + j > buf_len) {
                        lyxp_expr_free(exp);
                        LOGBUF(cur_expr);
                        return -1;
                    }
                    strncpy(&buf[count], cur_expr, j);
                    count += j;
                }
                module_name = cur_expr;

                /* copy the rest */
                if (count + (exp->tok_len[i] - j) > buf_len) {
                    lyxp_expr_free(exp);
                    LOGBUF(end);
                    return -1;
                }
                strncpy(&buf[count], end, exp->tok_len[i] - j);
                count += exp->tok_len[i] - j;
            } else {
                if (count + exp->tok_len[i] > buf_len) {
                    lyxp_expr_free(exp);
                    LOGBUF(&exp->expr[exp->expr_pos[i]]);
                    return -1;
                }
                strncpy(&buf[count], &exp->expr[exp->expr_pos[i]], exp->tok_len[i]);
                count += exp->tok_len[i];
            }
        }
        if (count > buf_len) {
            LOGINT(ctx);
            lyxp_expr_free(exp);
            return -1;
        }
        buf[count] = '\0';

        lyxp_expr_free(exp);
        break;

    case LY_TYPE_DEC64:
        num = *((int64_t *)data1);
        c = *((uint8_t *)data2);
        if (num) {
            count = sprintf(buf, "%"PRId64" ", num);
            if ( (num > 0 && (count - 1) <= c)
                 || (count - 2) <= c ) {
                /* we have 0. value, print the value with the leading zeros
                 * (one for 0. and also keep the correct with of num according
                 * to fraction-digits value)
                 * for (num<0) - extra character for '-' sign */
                count = sprintf(buf, "%0*"PRId64" ", (num > 0) ? (c + 1) : (c + 2), num);
            }
            for (i = c, j = 1; i > 0 ; i--) {
                if (j && i > 1 && buf[count - 2] == '0') {
                    /* we have trailing zero to skip */
                    buf[count - 1] = '\0';
                } else {
                    j = 0;
                    buf[count - 1] = buf[count - 2];
                }
                count--;
            }
            buf[count - 1] = '.';
        } else {
            /* zero */
            sprintf(buf, "0.0");
        }
        break;

    case LY_TYPE_INT8:
    case LY_TYPE_INT16:
    case LY_TYPE_INT32:
    case LY_TYPE_INT64:
        num = *((int64_t *)data1);
        sprintf(buf, "%"PRId64, num);
        break;

    case LY_TYPE_UINT8:
    case LY_TYPE_UINT16:
    case LY_TYPE_UINT32:
    case LY_TYPE_UINT64:
        unum = *((uint64_t *)data1);
        sprintf(buf, "%"PRIu64, unum);
        break;

    default:
        /* should not be even called - just do nothing */
        return 0;
    }

    if (strcmp(buf, *value)) {
        lydict_remove(ctx, *value);
        *value = lydict_insert(ctx, buf, 0);
        return 1;
    }

    return 0;

#undef LOGBUF
}