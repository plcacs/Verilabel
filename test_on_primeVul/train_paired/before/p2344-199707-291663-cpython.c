PyObject *PyString_DecodeEscape(const char *s,
                                Py_ssize_t len,
                                const char *errors,
                                Py_ssize_t unicode,
                                const char *recode_encoding)
{
    int c;
    char *p, *buf;
    const char *end;
    PyObject *v;
    Py_ssize_t newlen = recode_encoding ? 4*len:len;
    v = PyString_FromStringAndSize((char *)NULL, newlen);
    if (v == NULL)
        return NULL;
    p = buf = PyString_AsString(v);
    end = s + len;
    while (s < end) {
        if (*s != '\\') {
          non_esc:
#ifdef Py_USING_UNICODE
            if (recode_encoding && (*s & 0x80)) {
                PyObject *u, *w;
                char *r;
                const char* t;
                Py_ssize_t rn;
                t = s;
                /* Decode non-ASCII bytes as UTF-8. */
                while (t < end && (*t & 0x80)) t++;
                u = PyUnicode_DecodeUTF8(s, t - s, errors);
                if(!u) goto failed;

                /* Recode them in target encoding. */
                w = PyUnicode_AsEncodedString(
                    u, recode_encoding, errors);
                Py_DECREF(u);
                if (!w)                 goto failed;

                /* Append bytes to output buffer. */
                assert(PyString_Check(w));
                r = PyString_AS_STRING(w);
                rn = PyString_GET_SIZE(w);
                Py_MEMCPY(p, r, rn);
                p += rn;
                Py_DECREF(w);
                s = t;
            } else {
                *p++ = *s++;
            }
#else
            *p++ = *s++;
#endif
            continue;
        }
        s++;
        if (s==end) {
            PyErr_SetString(PyExc_ValueError,
                            "Trailing \\ in string");
            goto failed;
        }
        switch (*s++) {
        /* XXX This assumes ASCII! */
        case '\n': break;
        case '\\': *p++ = '\\'; break;
        case '\'': *p++ = '\''; break;
        case '\"': *p++ = '\"'; break;
        case 'b': *p++ = '\b'; break;
        case 'f': *p++ = '\014'; break; /* FF */
        case 't': *p++ = '\t'; break;
        case 'n': *p++ = '\n'; break;
        case 'r': *p++ = '\r'; break;
        case 'v': *p++ = '\013'; break; /* VT */
        case 'a': *p++ = '\007'; break; /* BEL, not classic C */
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7':
            c = s[-1] - '0';
            if (s < end && '0' <= *s && *s <= '7') {
                c = (c<<3) + *s++ - '0';
                if (s < end && '0' <= *s && *s <= '7')
                    c = (c<<3) + *s++ - '0';
            }
            *p++ = c;
            break;
        case 'x':
            if (s+1 < end &&
                isxdigit(Py_CHARMASK(s[0])) &&
                isxdigit(Py_CHARMASK(s[1])))
            {
                unsigned int x = 0;
                c = Py_CHARMASK(*s);
                s++;
                if (isdigit(c))
                    x = c - '0';
                else if (islower(c))
                    x = 10 + c - 'a';
                else
                    x = 10 + c - 'A';
                x = x << 4;
                c = Py_CHARMASK(*s);
                s++;
                if (isdigit(c))
                    x += c - '0';
                else if (islower(c))
                    x += 10 + c - 'a';
                else
                    x += 10 + c - 'A';
                *p++ = x;
                break;
            }
            if (!errors || strcmp(errors, "strict") == 0) {
                PyErr_SetString(PyExc_ValueError,
                                "invalid \\x escape");
                goto failed;
            }
            if (strcmp(errors, "replace") == 0) {
                *p++ = '?';
            } else if (strcmp(errors, "ignore") == 0)
                /* do nothing */;
            else {
                PyErr_Format(PyExc_ValueError,
                             "decoding error; "
                             "unknown error handling code: %.400s",
                             errors);
                goto failed;
            }
            /* skip \x */
            if (s < end && isxdigit(Py_CHARMASK(s[0])))
                s++; /* and a hexdigit */
            break;
#ifndef Py_USING_UNICODE
        case 'u':
        case 'U':
        case 'N':
            if (unicode) {
                PyErr_SetString(PyExc_ValueError,
                          "Unicode escapes not legal "
                          "when Unicode disabled");
                goto failed;
            }
#endif
        default:
            *p++ = '\\';
            s--;
            goto non_esc; /* an arbitrary number of unescaped
                             UTF-8 bytes may follow. */
        }
    }
    if (p-buf < newlen)
        _PyString_Resize(&v, p - buf); /* v is cleared on error */
    return v;
  failed:
    Py_DECREF(v);
    return NULL;
}