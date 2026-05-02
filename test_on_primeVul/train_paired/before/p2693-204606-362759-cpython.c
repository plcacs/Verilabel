handle_keywordonly_args(struct compiling *c, const node *n, int start,
                        asdl_seq *kwonlyargs, asdl_seq *kwdefaults)
{
    PyObject *argname;
    node *ch;
    expr_ty expression, annotation;
    arg_ty arg;
    int i = start;
    int j = 0; /* index for kwdefaults and kwonlyargs */

    if (kwonlyargs == NULL) {
        ast_error(c, CHILD(n, start), "named arguments must follow bare *");
        return -1;
    }
    assert(kwdefaults != NULL);
    while (i < NCH(n)) {
        ch = CHILD(n, i);
        switch (TYPE(ch)) {
            case vfpdef:
            case tfpdef:
                if (i + 1 < NCH(n) && TYPE(CHILD(n, i + 1)) == EQUAL) {
                    expression = ast_for_expr(c, CHILD(n, i + 2));
                    if (!expression)
                        goto error;
                    asdl_seq_SET(kwdefaults, j, expression);
                    i += 2; /* '=' and test */
                }
                else { /* setting NULL if no default value exists */
                    asdl_seq_SET(kwdefaults, j, NULL);
                }
                if (NCH(ch) == 3) {
                    /* ch is NAME ':' test */
                    annotation = ast_for_expr(c, CHILD(ch, 2));
                    if (!annotation)
                        goto error;
                }
                else {
                    annotation = NULL;
                }
                ch = CHILD(ch, 0);
                argname = NEW_IDENTIFIER(ch);
                if (!argname)
                    goto error;
                if (forbidden_name(c, argname, ch, 0))
                    goto error;
                arg = arg(argname, annotation, LINENO(ch), ch->n_col_offset,
                          ch->n_end_lineno, ch->n_end_col_offset,
                          c->c_arena);
                if (!arg)
                    goto error;
                asdl_seq_SET(kwonlyargs, j++, arg);
                i += 2; /* the name and the comma */
                break;
            case DOUBLESTAR:
                return i;
            default:
                ast_error(c, ch, "unexpected node");
                goto error;
        }
    }
    return i;
 error:
    return -1;
}