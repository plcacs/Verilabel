static bool php_mb_parse_encoding(const Variant& encoding,
                                  mbfl_encoding ***return_list,
                                  int *return_size, bool persistent) {
  bool ret;
  if (encoding.isArray()) {
    ret = php_mb_parse_encoding_array(encoding.toArray(),
                                      return_list, return_size,
                                      persistent ? 1 : 0);
  } else {
    String enc = encoding.toString();
    ret = php_mb_parse_encoding_list(enc.data(), enc.size(),
                                     return_list, return_size,
                                     persistent ? 1 : 0);
  }
  if (!ret) {
    if (return_list && *return_list) {
      free(*return_list);
      *return_list = nullptr;
    }
    return_size = 0;
  }
  return ret;
}