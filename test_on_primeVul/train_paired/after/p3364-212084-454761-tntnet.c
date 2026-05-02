  bool Messageheader::Parser::state_fieldbody_crlf(char ch)
  {
    if (ch == '\r')
      SET_STATE(state_end_cr);
    else if (ch == '\n')
    {
      log_debug("header " << fieldnamePtr << ": " << fieldbodyPtr);
      if (header.onField(fieldnamePtr, fieldbodyPtr) == FAIL)
      {
        failedFlag = true;
        log_warn("invalid character " << chartoprint(ch) << " in fieldbody");
      }

      *headerdataPtr = '\0';
      return true;
    }
    else if (std::isspace(ch))
    {
      // continuation line
      checkHeaderspace(1);
      *(headerdataPtr - 1) = '\n';
      *headerdataPtr++ = ch;
      SET_STATE(state_fieldbody);
    }
    else if (ch >= 33 && ch <= 126)
    {
      switch (header.onField(fieldnamePtr, fieldbodyPtr))
      {
        case OK:   SET_STATE(state_fieldname);
                   break;
        case FAIL: failedFlag = true;
                   log_warn("invalid character " << chartoprint(ch) << " in fieldbody");
                   break;
        case END:  return true;
                   break;
      }

      fieldnamePtr = headerdataPtr;
      checkHeaderspace(1);
      *headerdataPtr++ = ch;
    }
    return false;
  }