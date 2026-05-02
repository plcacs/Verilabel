static Variant _php_mb_regex_ereg_replace_exec(const Variant& pattern,
                                               const String& replacement,
                                               const String& str,
                                               const String& option,
                                               OnigOptionType options) {
  const char *p;
  php_mb_regex_t *re;
  OnigSyntaxType *syntax;
  OnigRegion *regs = nullptr;
  StringBuffer out_buf;
  int i, err, eval, n;
  OnigUChar *pos;
  OnigUChar *string_lim;
  char pat_buf[2];

  const mbfl_encoding *enc;

  {
    const char *current_enc_name;
    current_enc_name = php_mb_regex_mbctype2name(MBSTRG(current_mbctype));
    if (current_enc_name == nullptr ||
      (enc = mbfl_name2encoding(current_enc_name)) == nullptr) {
      raise_warning("Unknown error");
      return false;
    }
  }
  eval = 0;
  {
    if (!option.empty()) {
      _php_mb_regex_init_options(option.data(), option.size(),
                                 &options, &syntax, &eval);
    } else {
      options |= MBSTRG(regex_default_options);
      syntax = MBSTRG(regex_default_syntax);
    }
  }

  String spattern;
  if (pattern.isString()) {
    spattern = pattern.toString();
  } else {
    /* FIXME: this code is not multibyte aware! */
    pat_buf[0] = pattern.toByte();
    pat_buf[1] = '\0';
    spattern = String(pat_buf, 1, CopyString);
  }
  /* create regex pattern buffer */
  re = php_mbregex_compile_pattern(spattern, options,
                                   MBSTRG(current_mbctype), syntax);
  if (re == nullptr) {
    return false;
  }

  if (eval) {
    throw_not_supported("ereg_replace", "dynamic coding");
  }

  /* do the actual work */
  err = 0;
  pos = (OnigUChar*)str.data();
  string_lim = (OnigUChar*)(str.data() + str.size());
  regs = onig_region_new();
  while (err >= 0) {
    err = onig_search(re, (OnigUChar *)str.data(), (OnigUChar *)string_lim,
                      pos, (OnigUChar *)string_lim, regs, 0);
    if (err <= -2) {
      OnigUChar err_str[ONIG_MAX_ERROR_MESSAGE_LEN];
      onig_error_code_to_str(err_str, err);
      raise_warning("mbregex search failure: %s", err_str);
      break;
    }
    if (err >= 0) {
#if moriyoshi_0
      if (regs->beg[0] == regs->end[0]) {
        raise_warning("Empty regular expression");
        break;
      }
#endif
      /* copy the part of the string before the match */
      out_buf.append((const char *)pos,
                     (OnigUChar *)(str.data() + regs->beg[0]) - pos);
      /* copy replacement and backrefs */
      i = 0;
      p = replacement.data();
      while (i < replacement.size()) {
        int fwd = (int)php_mb_mbchar_bytes_ex(p, enc);
        n = -1;
        if ((replacement.size() - i) >= 2 && fwd == 1 &&
          p[0] == '\\' && p[1] >= '0' && p[1] <= '9') {
          n = p[1] - '0';
        }
        if (n >= 0 && n < regs->num_regs) {
          if (regs->beg[n] >= 0 && regs->beg[n] < regs->end[n] &&
              regs->end[n] <= str.size()) {
            out_buf.append(str.data() + regs->beg[n],
                           regs->end[n] - regs->beg[n]);
          }
          p += 2;
          i += 2;
        } else {
          out_buf.append(p, fwd);
          p += fwd;
          i += fwd;
        }
      }
      n = regs->end[0];
      if ((pos - (OnigUChar *)str.data()) < n) {
        pos = (OnigUChar *)(str.data() + n);
      } else {
        if (pos < string_lim) {
          out_buf.append((const char *)pos, 1);
        }
        pos++;
      }
    } else { /* nomatch */
      /* stick that last bit of string on our output */
      if (string_lim - pos > 0) {
        out_buf.append((const char *)pos, string_lim - pos);
      }
    }
    onig_region_free(regs, 0);
  }

  if (regs != nullptr) {
    onig_region_free(regs, 1);
  }

  if (err <= -2) {
    return false;
  }
  return out_buf.detach();
}