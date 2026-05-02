eval7(
    char_u	**arg,
    typval_T	*rettv,
    evalarg_T	*evalarg,
    int		want_string)	// after "." operator
{
    int		evaluate = evalarg != NULL
				      && (evalarg->eval_flags & EVAL_EVALUATE);
    int		len;
    char_u	*s;
    char_u	*name_start = NULL;
    char_u	*start_leader, *end_leader;
    int		ret = OK;
    char_u	*alias;

    /*
     * Initialise variable so that clear_tv() can't mistake this for a
     * string and free a string that isn't there.
     */
    rettv->v_type = VAR_UNKNOWN;

    /*
     * Skip '!', '-' and '+' characters.  They are handled later.
     */
    start_leader = *arg;
    if (eval_leader(arg, in_vim9script()) == FAIL)
	return FAIL;
    end_leader = *arg;

    if (**arg == '.' && (!isdigit(*(*arg + 1))
#ifdef FEAT_FLOAT
	    || in_old_script(2)
#endif
	    ))
    {
	semsg(_(e_invalid_expression_str), *arg);
	++*arg;
	return FAIL;
    }

    switch (**arg)
    {
    /*
     * Number constant.
     */
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '.':	ret = eval_number(arg, rettv, evaluate, want_string);

		// Apply prefixed "-" and "+" now.  Matters especially when
		// "->" follows.
		if (ret == OK && evaluate && end_leader > start_leader
						  && rettv->v_type != VAR_BLOB)
		    ret = eval7_leader(rettv, TRUE, start_leader, &end_leader);
		break;

    /*
     * String constant: "string".
     */
    case '"':	ret = eval_string(arg, rettv, evaluate);
		break;

    /*
     * Literal string constant: 'str''ing'.
     */
    case '\'':	ret = eval_lit_string(arg, rettv, evaluate);
		break;

    /*
     * List: [expr, expr]
     */
    case '[':	ret = eval_list(arg, rettv, evalarg, TRUE);
		break;

    /*
     * Dictionary: #{key: val, key: val}
     */
    case '#':	if (in_vim9script())
		{
		    ret = vim9_bad_comment(*arg) ? FAIL : NOTDONE;
		}
		else if ((*arg)[1] == '{')
		{
		    ++*arg;
		    ret = eval_dict(arg, rettv, evalarg, TRUE);
		}
		else
		    ret = NOTDONE;
		break;

    /*
     * Lambda: {arg, arg -> expr}
     * Dictionary: {'key': val, 'key': val}
     */
    case '{':	if (in_vim9script())
		    ret = NOTDONE;
		else
		    ret = get_lambda_tv(arg, rettv, in_vim9script(), evalarg);
		if (ret == NOTDONE)
		    ret = eval_dict(arg, rettv, evalarg, FALSE);
		break;

    /*
     * Option value: &name
     */
    case '&':	ret = eval_option(arg, rettv, evaluate);
		break;

    /*
     * Environment variable: $VAR.
     */
    case '$':	ret = eval_env_var(arg, rettv, evaluate);
		break;

    /*
     * Register contents: @r.
     */
    case '@':	++*arg;
		if (evaluate)
		{
		    if (in_vim9script() && IS_WHITE_OR_NUL(**arg))
			semsg(_(e_syntax_error_at_str), *arg);
		    else if (in_vim9script() && !valid_yank_reg(**arg, FALSE))
			emsg_invreg(**arg);
		    else
		    {
			rettv->v_type = VAR_STRING;
			rettv->vval.v_string = get_reg_contents(**arg,
								GREG_EXPR_SRC);
		    }
		}
		if (**arg != NUL)
		    ++*arg;
		break;

    /*
     * nested expression: (expression).
     * or lambda: (arg) => expr
     */
    case '(':	ret = NOTDONE;
		if (in_vim9script())
		{
		    ret = get_lambda_tv(arg, rettv, TRUE, evalarg);
		    if (ret == OK && evaluate)
		    {
			ufunc_T *ufunc = rettv->vval.v_partial->pt_func;

			// Compile it here to get the return type.  The return
			// type is optional, when it's missing use t_unknown.
			// This is recognized in compile_return().
			if (ufunc->uf_ret_type->tt_type == VAR_VOID)
			    ufunc->uf_ret_type = &t_unknown;
			if (compile_def_function(ufunc,
				     FALSE, COMPILE_TYPE(ufunc), NULL) == FAIL)
			{
			    clear_tv(rettv);
			    ret = FAIL;
			}
		    }
		}
		if (ret == NOTDONE)
		{
		    *arg = skipwhite_and_linebreak(*arg + 1, evalarg);
		    ret = eval1(arg, rettv, evalarg);	// recursive!

		    *arg = skipwhite_and_linebreak(*arg, evalarg);
		    if (**arg == ')')
			++*arg;
		    else if (ret == OK)
		    {
			emsg(_(e_missing_closing_paren));
			clear_tv(rettv);
			ret = FAIL;
		    }
		}
		break;

    default:	ret = NOTDONE;
		break;
    }

    if (ret == NOTDONE)
    {
	/*
	 * Must be a variable or function name.
	 * Can also be a curly-braces kind of name: {expr}.
	 */
	s = *arg;
	len = get_name_len(arg, &alias, evaluate, TRUE);
	if (alias != NULL)
	    s = alias;

	if (len <= 0)
	    ret = FAIL;
	else
	{
	    int	    flags = evalarg == NULL ? 0 : evalarg->eval_flags;

	    if (evaluate && in_vim9script() && len == 1 && *s == '_')
	    {
		emsg(_(e_cannot_use_underscore_here));
		ret = FAIL;
	    }
	    else if ((in_vim9script() ? **arg : *skipwhite(*arg)) == '(')
	    {
		// "name(..."  recursive!
		*arg = skipwhite(*arg);
		ret = eval_func(arg, evalarg, s, len, rettv, flags, NULL);
	    }
	    else if (flags & EVAL_CONSTANT)
		ret = FAIL;
	    else if (evaluate)
	    {
		// get the value of "true", "false" or a variable
		if (len == 4 && in_vim9script() && STRNCMP(s, "true", 4) == 0)
		{
		    rettv->v_type = VAR_BOOL;
		    rettv->vval.v_number = VVAL_TRUE;
		    ret = OK;
		}
		else if (len == 5 && in_vim9script()
						&& STRNCMP(s, "false", 5) == 0)
		{
		    rettv->v_type = VAR_BOOL;
		    rettv->vval.v_number = VVAL_FALSE;
		    ret = OK;
		}
		else if (len == 4 && in_vim9script()
						&& STRNCMP(s, "null", 4) == 0)
		{
		    rettv->v_type = VAR_SPECIAL;
		    rettv->vval.v_number = VVAL_NULL;
		    ret = OK;
		}
		else
		{
		    name_start = s;
		    ret = eval_variable(s, len, 0, rettv, NULL,
					   EVAL_VAR_VERBOSE + EVAL_VAR_IMPORT);
		}
	    }
	    else
	    {
		// skip the name
		check_vars(s, len);
		ret = OK;
	    }
	}
	vim_free(alias);
    }

    // Handle following '[', '(' and '.' for expr[expr], expr.name,
    // expr(expr), expr->name(expr)
    if (ret == OK)
	ret = handle_subscript(arg, name_start, rettv, evalarg, TRUE);

    /*
     * Apply logical NOT and unary '-', from right to left, ignore '+'.
     */
    if (ret == OK && evaluate && end_leader > start_leader)
	ret = eval7_leader(rettv, FALSE, start_leader, &end_leader);
    return ret;
}