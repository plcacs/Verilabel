int string_rfind(const char *input, int len, const char *s, int s_len,
                 int pos, bool case_sensitive) {
  assertx(input);
  assertx(s);
  if (!s_len || pos < -len || pos > len) {
    return -1;
  }
  void *ptr;
  if (case_sensitive) {
    if (pos >= 0) {
      ptr = bstrrstr(input + pos, len - pos, s, s_len);
    } else {
      ptr = bstrrstr(input, len + pos + s_len, s, s_len);
    }
  } else {
    if (pos >= 0) {
      ptr = bstrrcasestr(input + pos, len - pos, s, s_len);
    } else {
      ptr = bstrrcasestr(input, len + pos + s_len, s, s_len);
    }
  }
  if (ptr != nullptr) {
    return (int)((const char *)ptr - input);
  }
  return -1;
}