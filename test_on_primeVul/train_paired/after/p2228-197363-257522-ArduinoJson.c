char *QuotedString::extractFrom(char *input, char **endPtr) {
  char *startPtr = input + 1;  // skip the quote
  char *readPtr = startPtr;
  char *writePtr = startPtr;
  char c;

  char firstChar = *input;
  char stopChar = firstChar;  // closing quote is the same as opening quote

  if (!isQuote(firstChar)) goto ERROR_OPENING_QUOTE_MISSING;

  for (;;) {
    c = *readPtr++;

    if (c == '\0') goto ERROR_CLOSING_QUOTE_MISSING;

    if (c == stopChar) goto SUCCESS;

    if (c == '\\') {
      // replace char
      c = unescapeChar(*readPtr++);
      if (c == '\0') goto ERROR_ESCAPE_SEQUENCE_INTERRUPTED;
    }

    *writePtr++ = c;
  }

SUCCESS:
  // end the string here
  *writePtr = '\0';

  // update end ptr
  *endPtr = readPtr;

  // return pointer to unquoted string
  return startPtr;

ERROR_OPENING_QUOTE_MISSING:
ERROR_CLOSING_QUOTE_MISSING:
ERROR_ESCAPE_SEQUENCE_INTERRUPTED:
  return NULL;
}