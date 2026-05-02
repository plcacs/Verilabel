char *QuotedString::extractFrom(char *input, char **endPtr) {
  char firstChar = *input;

  if (!isQuote(firstChar)) {
    // must start with a quote
    return NULL;
  }

  char stopChar = firstChar;  // closing quote is the same as opening quote

  char *startPtr = input + 1;  // skip the quote
  char *readPtr = startPtr;
  char *writePtr = startPtr;
  char c;

  for (;;) {
    c = *readPtr++;

    if (c == '\0') {
      // premature ending
      return NULL;
    }

    if (c == stopChar) {
      // closing quote
      break;
    }

    if (c == '\\') {
      // replace char
      c = unescapeChar(*readPtr++);
    }

    *writePtr++ = c;
  }

  // end the string here
  *writePtr = '\0';

  // update end ptr
  *endPtr = readPtr;

  return startPtr;
}