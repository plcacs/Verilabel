parse_reg_exp (re_string_t *regexp, regex_t *preg, re_token_t *token,
	       reg_syntax_t syntax, Idx nest, reg_errcode_t *err)
{
  re_dfa_t *dfa = preg->buffer;
  bin_tree_t *tree, *branch = NULL;
  tree = parse_branch (regexp, preg, token, syntax, nest, err);
  if (BE (*err != REG_NOERROR && tree == NULL, 0))
    return NULL;

  while (token->type == OP_ALT)
    {
      fetch_token (token, regexp, syntax | RE_CARET_ANCHORS_HERE);
      if (token->type != OP_ALT && token->type != END_OF_RE
	  && (nest == 0 || token->type != OP_CLOSE_SUBEXP))
	{
	  branch = parse_branch (regexp, preg, token, syntax, nest, err);
	  if (BE (*err != REG_NOERROR && branch == NULL, 0))
	    {
	      if (tree != NULL)
		postorder (tree, free_tree, NULL);
	      return NULL;
	    }
	}
      else
	branch = NULL;
      tree = create_tree (dfa, tree, branch, OP_ALT);
      if (BE (tree == NULL, 0))
	{
	  *err = REG_ESPACE;
	  return NULL;
	}
    }
  return tree;
}