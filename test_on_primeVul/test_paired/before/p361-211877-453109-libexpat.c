addBinding(XML_Parser parser, PREFIX *prefix, const ATTRIBUTE_ID *attId,
           const XML_Char *uri, BINDING **bindingsPtr) {
  static const XML_Char xmlNamespace[]
      = {ASCII_h,      ASCII_t,     ASCII_t,     ASCII_p,      ASCII_COLON,
         ASCII_SLASH,  ASCII_SLASH, ASCII_w,     ASCII_w,      ASCII_w,
         ASCII_PERIOD, ASCII_w,     ASCII_3,     ASCII_PERIOD, ASCII_o,
         ASCII_r,      ASCII_g,     ASCII_SLASH, ASCII_X,      ASCII_M,
         ASCII_L,      ASCII_SLASH, ASCII_1,     ASCII_9,      ASCII_9,
         ASCII_8,      ASCII_SLASH, ASCII_n,     ASCII_a,      ASCII_m,
         ASCII_e,      ASCII_s,     ASCII_p,     ASCII_a,      ASCII_c,
         ASCII_e,      '\0'};
  static const int xmlLen = (int)sizeof(xmlNamespace) / sizeof(XML_Char) - 1;
  static const XML_Char xmlnsNamespace[]
      = {ASCII_h,     ASCII_t,      ASCII_t, ASCII_p, ASCII_COLON,  ASCII_SLASH,
         ASCII_SLASH, ASCII_w,      ASCII_w, ASCII_w, ASCII_PERIOD, ASCII_w,
         ASCII_3,     ASCII_PERIOD, ASCII_o, ASCII_r, ASCII_g,      ASCII_SLASH,
         ASCII_2,     ASCII_0,      ASCII_0, ASCII_0, ASCII_SLASH,  ASCII_x,
         ASCII_m,     ASCII_l,      ASCII_n, ASCII_s, ASCII_SLASH,  '\0'};
  static const int xmlnsLen
      = (int)sizeof(xmlnsNamespace) / sizeof(XML_Char) - 1;

  XML_Bool mustBeXML = XML_FALSE;
  XML_Bool isXML = XML_TRUE;
  XML_Bool isXMLNS = XML_TRUE;

  BINDING *b;
  int len;

  /* empty URI is only valid for default namespace per XML NS 1.0 (not 1.1) */
  if (*uri == XML_T('\0') && prefix->name)
    return XML_ERROR_UNDECLARING_PREFIX;

  if (prefix->name && prefix->name[0] == XML_T(ASCII_x)
      && prefix->name[1] == XML_T(ASCII_m)
      && prefix->name[2] == XML_T(ASCII_l)) {
    /* Not allowed to bind xmlns */
    if (prefix->name[3] == XML_T(ASCII_n) && prefix->name[4] == XML_T(ASCII_s)
        && prefix->name[5] == XML_T('\0'))
      return XML_ERROR_RESERVED_PREFIX_XMLNS;

    if (prefix->name[3] == XML_T('\0'))
      mustBeXML = XML_TRUE;
  }

  for (len = 0; uri[len]; len++) {
    if (isXML && (len > xmlLen || uri[len] != xmlNamespace[len]))
      isXML = XML_FALSE;

    if (! mustBeXML && isXMLNS
        && (len > xmlnsLen || uri[len] != xmlnsNamespace[len]))
      isXMLNS = XML_FALSE;
  }
  isXML = isXML && len == xmlLen;
  isXMLNS = isXMLNS && len == xmlnsLen;

  if (mustBeXML != isXML)
    return mustBeXML ? XML_ERROR_RESERVED_PREFIX_XML
                     : XML_ERROR_RESERVED_NAMESPACE_URI;

  if (isXMLNS)
    return XML_ERROR_RESERVED_NAMESPACE_URI;

  if (parser->m_namespaceSeparator)
    len++;
  if (parser->m_freeBindingList) {
    b = parser->m_freeBindingList;
    if (len > b->uriAlloc) {
      /* Detect and prevent integer overflow */
      if (len > INT_MAX - EXPAND_SPARE) {
        return XML_ERROR_NO_MEMORY;
      }

      /* Detect and prevent integer overflow.
       * The preprocessor guard addresses the "always false" warning
       * from -Wtype-limits on platforms where
       * sizeof(unsigned int) < sizeof(size_t), e.g. on x86_64. */
#if UINT_MAX >= SIZE_MAX
      if ((unsigned)(len + EXPAND_SPARE) > (size_t)(-1) / sizeof(XML_Char)) {
        return XML_ERROR_NO_MEMORY;
      }
#endif

      XML_Char *temp = (XML_Char *)REALLOC(
          parser, b->uri, sizeof(XML_Char) * (len + EXPAND_SPARE));
      if (temp == NULL)
        return XML_ERROR_NO_MEMORY;
      b->uri = temp;
      b->uriAlloc = len + EXPAND_SPARE;
    }
    parser->m_freeBindingList = b->nextTagBinding;
  } else {
    b = (BINDING *)MALLOC(parser, sizeof(BINDING));
    if (! b)
      return XML_ERROR_NO_MEMORY;

    /* Detect and prevent integer overflow */
    if (len > INT_MAX - EXPAND_SPARE) {
      return XML_ERROR_NO_MEMORY;
    }
    /* Detect and prevent integer overflow.
     * The preprocessor guard addresses the "always false" warning
     * from -Wtype-limits on platforms where
     * sizeof(unsigned int) < sizeof(size_t), e.g. on x86_64. */
#if UINT_MAX >= SIZE_MAX
    if ((unsigned)(len + EXPAND_SPARE) > (size_t)(-1) / sizeof(XML_Char)) {
      return XML_ERROR_NO_MEMORY;
    }
#endif

    b->uri
        = (XML_Char *)MALLOC(parser, sizeof(XML_Char) * (len + EXPAND_SPARE));
    if (! b->uri) {
      FREE(parser, b);
      return XML_ERROR_NO_MEMORY;
    }
    b->uriAlloc = len + EXPAND_SPARE;
  }
  b->uriLen = len;
  memcpy(b->uri, uri, len * sizeof(XML_Char));
  if (parser->m_namespaceSeparator)
    b->uri[len - 1] = parser->m_namespaceSeparator;
  b->prefix = prefix;
  b->attId = attId;
  b->prevPrefixBinding = prefix->binding;
  /* NULL binding when default namespace undeclared */
  if (*uri == XML_T('\0') && prefix == &parser->m_dtd->defaultPrefix)
    prefix->binding = NULL;
  else
    prefix->binding = b;
  b->nextTagBinding = *bindingsPtr;
  *bindingsPtr = b;
  /* if attId == NULL then we are not starting a namespace scope */
  if (attId && parser->m_startNamespaceDeclHandler)
    parser->m_startNamespaceDeclHandler(parser->m_handlerArg, prefix->name,
                                        prefix->binding ? uri : 0);
  return XML_ERROR_NONE;
}