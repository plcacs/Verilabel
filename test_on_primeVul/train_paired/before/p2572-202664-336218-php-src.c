compile_string_node(Node* node, regex_t* reg)
{
  int r, len, prev_len, slen, ambig;
  OnigEncoding enc = reg->enc;
  UChar *p, *prev, *end;
  StrNode* sn;

  sn = NSTR(node);
  if (sn->end <= sn->s)
    return 0;

  end = sn->end;
  ambig = NSTRING_IS_AMBIG(node);

  p = prev = sn->s;
  prev_len = enclen(enc, p);
  p += prev_len;
  slen = 1;

  for (; p < end; ) {
    len = enclen(enc, p);
    if (len == prev_len) {
      slen++;
    }
    else {
      r = add_compile_string(prev, prev_len, slen, reg, ambig);
      if (r) return r;

      prev  = p;
      slen  = 1;
      prev_len = len;
    }

    p += len;
  }
  return add_compile_string(prev, prev_len, slen, reg, ambig);
}