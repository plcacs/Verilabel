fetch_interval_quantifier(UChar** src, UChar* end, PToken* tok, ScanEnv* env)
{
  int low, up, syn_allow, non_low = 0;
  int r = 0;
  OnigCodePoint c;
  OnigEncoding enc = env->enc;
  UChar* p = *src;
  PFETCH_READY;

  syn_allow = IS_SYNTAX_BV(env->syntax, ONIG_SYN_ALLOW_INVALID_INTERVAL);

  if (PEND) {
    if (syn_allow)
      return 1;  /* "....{" : OK! */
    else
      return ONIGERR_END_PATTERN_AT_LEFT_BRACE;  /* "....{" syntax error */
  }

  if (! syn_allow) {
    c = PPEEK;
    if (c == ')' || c == '(' || c == '|') {
      return ONIGERR_END_PATTERN_AT_LEFT_BRACE;
    }
  }

  low = scan_number(&p, end, env->enc);
  if (low < 0) return ONIGERR_TOO_BIG_NUMBER_FOR_REPEAT_RANGE;
  if (low > ONIG_MAX_REPEAT_NUM)
    return ONIGERR_TOO_BIG_NUMBER_FOR_REPEAT_RANGE;

  if (p == *src) { /* can't read low */
    if (IS_SYNTAX_BV(env->syntax, ONIG_SYN_ALLOW_INTERVAL_LOW_ABBREV)) {
      /* allow {,n} as {0,n} */
      low = 0;
      non_low = 1;
    }
    else
      goto invalid;
  }

  if (PEND) goto invalid;
  PFETCH(c);
  if (c == ',') {
    UChar* prev = p;
    up = scan_number(&p, end, env->enc);
    if (up < 0) return ONIGERR_TOO_BIG_NUMBER_FOR_REPEAT_RANGE;
    if (up > ONIG_MAX_REPEAT_NUM)
      return ONIGERR_TOO_BIG_NUMBER_FOR_REPEAT_RANGE;

    if (p == prev) {
      if (non_low != 0)
        goto invalid;
      up = INFINITE_REPEAT;  /* {n,} : {n,infinite} */
    }
  }
  else {
    if (non_low != 0)
      goto invalid;

    PUNFETCH;
    up = low;  /* {n} : exact n times */
    r = 2;     /* fixed */
  }

  if (PEND) goto invalid;
  PFETCH(c);
  if (IS_SYNTAX_OP(env->syntax, ONIG_SYN_OP_ESC_BRACE_INTERVAL)) {
    if (c != MC_ESC(env->syntax)) goto invalid;
    PFETCH(c);
  }
  if (c != '}') goto invalid;

  if (!IS_INFINITE_REPEAT(up) && low > up) {
    /* {n,m}+ supported case */
    if (IS_SYNTAX_OP2(env->syntax, ONIG_SYN_OP2_PLUS_POSSESSIVE_INTERVAL))
      return ONIGERR_UPPER_SMALLER_THAN_LOWER_IN_REPEAT_RANGE;

    tok->u.repeat.possessive = 1;
    {
      int tmp;
      tmp = low; low = up; up = tmp;
    }
  }
  else
    tok->u.repeat.possessive = 0;

  tok->type = TK_INTERVAL;
  tok->u.repeat.lower = low;
  tok->u.repeat.upper = up;
  *src = p;
  return r; /* 0: normal {n,m}, 2: fixed {n} */

 invalid:
  if (syn_allow) {
    /* *src = p; */ /* !!! Don't do this line !!! */
    return 1;  /* OK */
  }
  else
    return ONIGERR_INVALID_REPEAT_RANGE_PATTERN;
}