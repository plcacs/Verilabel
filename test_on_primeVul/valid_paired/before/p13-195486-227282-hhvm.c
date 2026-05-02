  bool handleBackslash(signed char& out) {
    char ch = *p++;
    switch (ch) {
      case 0: return false;
      case '"': out = ch; return true;
      case '\\': out = ch; return true;
      case '/': out = ch; return true;
      case 'b': out = '\b'; return true;
      case 'f': out = '\f'; return true;
      case 'n': out = '\n'; return true;
      case 'r': out = '\r'; return true;
      case 't': out = '\t'; return true;
      case 'u': {
        if (UNLIKELY(is_tsimplejson)) {
          auto const ch1 = *p++;
          auto const ch2 = *p++;
          auto const dch3 = dehexchar(*p++);
          auto const dch4 = dehexchar(*p++);
          if (UNLIKELY(ch1 != '0' || ch2 != '0' || dch3 < 0 || dch4 < 0)) {
            return false;
          }
          out = (dch3 << 4) | dch4;
          return true;
        } else {
          uint16_t u16cp = 0;
          for (int i = 0; i < 4; i++) {
            auto const hexv = dehexchar(*p++);
            if (hexv < 0) return false; // includes check for end of string
            u16cp <<= 4;
            u16cp |= hexv;
          }
          if (u16cp > 0x7f) {
            return false;
          } else {
            out = u16cp;
            return true;
          }
        }
      }
      default: return false;
    }
  }