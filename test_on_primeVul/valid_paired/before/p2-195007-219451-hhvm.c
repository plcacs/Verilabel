static int xbuf_format_converter(char **outbuf, const char *fmt, va_list ap)
{
  register char *s = nullptr;
  char *q;
  int s_len;

  register int min_width = 0;
  int precision = 0;
  enum {
    LEFT, RIGHT
  } adjust;
  char pad_char;
  char prefix_char;

  double fp_num;
  wide_int i_num = (wide_int) 0;
  u_wide_int ui_num;

  char num_buf[NUM_BUF_SIZE];
  char char_buf[2];      /* for printing %% and %<unknown> */

#ifdef HAVE_LOCALE_H
  struct lconv *lconv = nullptr;
#endif

  /*
   * Flag variables
   */
  length_modifier_e modifier;
  boolean_e alternate_form;
  boolean_e print_sign;
  boolean_e print_blank;
  boolean_e adjust_precision;
  boolean_e adjust_width;
  int is_negative;

  int size = 240;
  char *result = (char *)malloc(size);
  int outpos = 0;

  while (*fmt) {
    if (*fmt != '%') {
      appendchar(&result, &outpos, &size, *fmt);
    } else {
      /*
       * Default variable settings
       */
      adjust = RIGHT;
      alternate_form = print_sign = print_blank = NO;
      pad_char = ' ';
      prefix_char = NUL;

      fmt++;

      /*
       * Try to avoid checking for flags, width or precision
       */
      if (isascii((int)*fmt) && !islower((int)*fmt)) {
        /*
         * Recognize flags: -, #, BLANK, +
         */
        for (;; fmt++) {
          if (*fmt == '-')
            adjust = LEFT;
          else if (*fmt == '+')
            print_sign = YES;
          else if (*fmt == '#')
            alternate_form = YES;
          else if (*fmt == ' ')
            print_blank = YES;
          else if (*fmt == '0')
            pad_char = '0';
          else
            break;
        }

        /*
         * Check if a width was specified
         */
        if (isdigit((int)*fmt)) {
          STR_TO_DEC(fmt, min_width);
          adjust_width = YES;
        } else if (*fmt == '*') {
          min_width = va_arg(ap, int);
          fmt++;
          adjust_width = YES;
          if (min_width < 0) {
            adjust = LEFT;
            min_width = -min_width;
          }
        } else
          adjust_width = NO;

        /*
         * Check if a precision was specified
         *
         * XXX: an unreasonable amount of precision may be specified
         * resulting in overflow of num_buf. Currently we
         * ignore this possibility.
         */
        if (*fmt == '.') {
          adjust_precision = YES;
          fmt++;
          if (isdigit((int)*fmt)) {
            STR_TO_DEC(fmt, precision);
          } else if (*fmt == '*') {
            precision = va_arg(ap, int);
            fmt++;
            if (precision < 0)
              precision = 0;
          } else
            precision = 0;
        } else
          adjust_precision = NO;
      } else
        adjust_precision = adjust_width = NO;

      /*
       * Modifier check
       */
      switch (*fmt) {
        case 'L':
          fmt++;
          modifier = LM_LONG_DOUBLE;
          break;
        case 'I':
          fmt++;
#if SIZEOF_LONG_LONG
          if (*fmt == '6' && *(fmt+1) == '4') {
            fmt += 2;
            modifier = LM_LONG_LONG;
          } else
#endif
            if (*fmt == '3' && *(fmt+1) == '2') {
              fmt += 2;
              modifier = LM_LONG;
            } else {
#ifdef _WIN64
              modifier = LM_LONG_LONG;
#else
              modifier = LM_LONG;
#endif
            }
          break;
        case 'l':
          fmt++;
#if SIZEOF_LONG_LONG
          if (*fmt == 'l') {
            fmt++;
            modifier = LM_LONG_LONG;
          } else
#endif
            modifier = LM_LONG;
          break;
        case 'z':
          fmt++;
          modifier = LM_SIZE_T;
          break;
        case 'j':
          fmt++;
#if SIZEOF_INTMAX_T
          modifier = LM_INTMAX_T;
#else
          modifier = LM_SIZE_T;
#endif
          break;
        case 't':
          fmt++;
#if SIZEOF_PTRDIFF_T
          modifier = LM_PTRDIFF_T;
#else
          modifier = LM_SIZE_T;
#endif
          break;
        case 'h':
          fmt++;
          if (*fmt == 'h') {
            fmt++;
          }
          /* these are promoted to int, so no break */
        default:
          modifier = LM_STD;
          break;
      }

      /*
       * Argument extraction and printing.
       * First we determine the argument type.
       * Then, we convert the argument to a string.
       * On exit from the switch, s points to the string that
       * must be printed, s_len has the length of the string
       * The precision requirements, if any, are reflected in s_len.
       *
       * NOTE: pad_char may be set to '0' because of the 0 flag.
       *   It is reset to ' ' by non-numeric formats
       */
      switch (*fmt) {
        case 'u':
          switch(modifier) {
            default:
              i_num = (wide_int) va_arg(ap, unsigned int);
              break;
            case LM_LONG_DOUBLE:
              goto fmt_error;
            case LM_LONG:
              i_num = (wide_int) va_arg(ap, unsigned long int);
              break;
            case LM_SIZE_T:
              i_num = (wide_int) va_arg(ap, size_t);
              break;
#if SIZEOF_LONG_LONG
            case LM_LONG_LONG:
              i_num = (wide_int) va_arg(ap, u_wide_int);
              break;
#endif
#if SIZEOF_INTMAX_T
            case LM_INTMAX_T:
              i_num = (wide_int) va_arg(ap, uintmax_t);
              break;
#endif
#if SIZEOF_PTRDIFF_T
            case LM_PTRDIFF_T:
              i_num = (wide_int) va_arg(ap, ptrdiff_t);
              break;
#endif
          }
          /*
           * The rest also applies to other integer formats, so fall
           * into that case.
           */
        case 'd':
        case 'i':
          /*
           * Get the arg if we haven't already.
           */
          if ((*fmt) != 'u') {
            switch(modifier) {
              default:
                i_num = (wide_int) va_arg(ap, int);
                break;
              case LM_LONG_DOUBLE:
                goto fmt_error;
              case LM_LONG:
                i_num = (wide_int) va_arg(ap, long int);
                break;
              case LM_SIZE_T:
#if SIZEOF_SSIZE_T
                i_num = (wide_int) va_arg(ap, ssize_t);
#else
                i_num = (wide_int) va_arg(ap, size_t);
#endif
                break;
#if SIZEOF_LONG_LONG
              case LM_LONG_LONG:
                i_num = (wide_int) va_arg(ap, wide_int);
                break;
#endif
#if SIZEOF_INTMAX_T
              case LM_INTMAX_T:
                i_num = (wide_int) va_arg(ap, intmax_t);
                break;
#endif
#if SIZEOF_PTRDIFF_T
              case LM_PTRDIFF_T:
                i_num = (wide_int) va_arg(ap, ptrdiff_t);
                break;
#endif
            }
          }
          s = ap_php_conv_10(i_num, (*fmt) == 'u', &is_negative,
                &num_buf[NUM_BUF_SIZE], &s_len);
          FIX_PRECISION(adjust_precision, precision, s, s_len);

          if (*fmt != 'u') {
            if (is_negative)
              prefix_char = '-';
            else if (print_sign)
              prefix_char = '+';
            else if (print_blank)
              prefix_char = ' ';
          }
          break;


        case 'o':
          switch(modifier) {
            default:
              ui_num = (u_wide_int) va_arg(ap, unsigned int);
              break;
            case LM_LONG_DOUBLE:
              goto fmt_error;
            case LM_LONG:
              ui_num = (u_wide_int) va_arg(ap, unsigned long int);
              break;
            case LM_SIZE_T:
              ui_num = (u_wide_int) va_arg(ap, size_t);
              break;
#if SIZEOF_LONG_LONG
            case LM_LONG_LONG:
              ui_num = (u_wide_int) va_arg(ap, u_wide_int);
              break;
#endif
#if SIZEOF_INTMAX_T
            case LM_INTMAX_T:
              ui_num = (u_wide_int) va_arg(ap, uintmax_t);
              break;
#endif
#if SIZEOF_PTRDIFF_T
            case LM_PTRDIFF_T:
              ui_num = (u_wide_int) va_arg(ap, ptrdiff_t);
              break;
#endif
          }
          s = ap_php_conv_p2(ui_num, 3, *fmt,
                &num_buf[NUM_BUF_SIZE], &s_len);
          FIX_PRECISION(adjust_precision, precision, s, s_len);
          if (alternate_form && *s != '0') {
            *--s = '0';
            s_len++;
          }
          break;


        case 'x':
        case 'X':
          switch(modifier) {
            default:
              ui_num = (u_wide_int) va_arg(ap, unsigned int);
              break;
            case LM_LONG_DOUBLE:
              goto fmt_error;
            case LM_LONG:
              ui_num = (u_wide_int) va_arg(ap, unsigned long int);
              break;
            case LM_SIZE_T:
              ui_num = (u_wide_int) va_arg(ap, size_t);
              break;
#if SIZEOF_LONG_LONG
            case LM_LONG_LONG:
              ui_num = (u_wide_int) va_arg(ap, u_wide_int);
              break;
#endif
#if SIZEOF_INTMAX_T
            case LM_INTMAX_T:
              ui_num = (u_wide_int) va_arg(ap, uintmax_t);
              break;
#endif
#if SIZEOF_PTRDIFF_T
            case LM_PTRDIFF_T:
              ui_num = (u_wide_int) va_arg(ap, ptrdiff_t);
              break;
#endif
          }
          s = ap_php_conv_p2(ui_num, 4, *fmt,
                &num_buf[NUM_BUF_SIZE], &s_len);
          FIX_PRECISION(adjust_precision, precision, s, s_len);
          if (alternate_form && i_num != 0) {
            *--s = *fmt;  /* 'x' or 'X' */
            *--s = '0';
            s_len += 2;
          }
          break;


        case 's':
        case 'v':
          s = va_arg(ap, char *);
          if (s != nullptr) {
            s_len = strlen(s);
            if (adjust_precision && precision < s_len)
              s_len = precision;
          } else {
            s = const_cast<char*>(s_null);
            s_len = S_NULL_LEN;
          }
          pad_char = ' ';
          break;


        case 'f':
        case 'F':
        case 'e':
        case 'E':
          switch(modifier) {
            case LM_LONG_DOUBLE:
              fp_num = (double) va_arg(ap, long double);
              break;
            case LM_STD:
              fp_num = va_arg(ap, double);
              break;
            default:
              goto fmt_error;
          }

          if (std::isnan(fp_num)) {
            s = const_cast<char*>("nan");
            s_len = 3;
          } else if (std::isinf(fp_num)) {
            s = const_cast<char*>("inf");
            s_len = 3;
          } else {
#ifdef HAVE_LOCALE_H
            if (!lconv) {
              lconv = localeconv();
            }
#endif
            s = php_conv_fp((*fmt == 'f')?'F':*fmt, fp_num, alternate_form,
             (adjust_precision == NO) ? FLOAT_DIGITS : precision,
             (*fmt == 'f')?LCONV_DECIMAL_POINT:'.',
                  &is_negative, &num_buf[1], &s_len);
            if (is_negative)
              prefix_char = '-';
            else if (print_sign)
              prefix_char = '+';
            else if (print_blank)
              prefix_char = ' ';
          }
          break;


        case 'g':
        case 'k':
        case 'G':
        case 'H':
          switch(modifier) {
            case LM_LONG_DOUBLE:
              fp_num = (double) va_arg(ap, long double);
              break;
            case LM_STD:
              fp_num = va_arg(ap, double);
              break;
            default:
              goto fmt_error;
          }

          if (std::isnan(fp_num)) {
             s = const_cast<char*>("NAN");
             s_len = 3;
             break;
           } else if (std::isinf(fp_num)) {
             if (fp_num > 0) {
               s = const_cast<char*>("INF");
               s_len = 3;
             } else {
               s = const_cast<char*>("-INF");
               s_len = 4;
             }
             break;
           }

          if (adjust_precision == NO)
            precision = FLOAT_DIGITS;
          else if (precision == 0)
            precision = 1;
          /*
           * * We use &num_buf[ 1 ], so that we have room for the sign
           */
#ifdef HAVE_LOCALE_H
          if (!lconv) {
            lconv = localeconv();
          }
#endif
          s = php_gcvt(fp_num, precision,
                       (*fmt=='H' || *fmt == 'k') ? '.' : LCONV_DECIMAL_POINT,
                       (*fmt == 'G' || *fmt == 'H')?'E':'e', &num_buf[1]);
          if (*s == '-')
            prefix_char = *s++;
          else if (print_sign)
            prefix_char = '+';
          else if (print_blank)
            prefix_char = ' ';

          s_len = strlen(s);

          if (alternate_form && (q = strchr(s, '.')) == nullptr)
            s[s_len++] = '.';
          break;


        case 'c':
          char_buf[0] = (char) (va_arg(ap, int));
          s = &char_buf[0];
          s_len = 1;
          pad_char = ' ';
          break;


        case '%':
          char_buf[0] = '%';
          s = &char_buf[0];
          s_len = 1;
          pad_char = ' ';
          break;


        case 'n':
          *(va_arg(ap, int *)) = outpos;
          goto skip_output;

          /*
           * Always extract the argument as a "char *" pointer. We
           * should be using "void *" but there are still machines
           * that don't understand it.
           * If the pointer size is equal to the size of an unsigned
           * integer we convert the pointer to a hex number, otherwise
           * we print "%p" to indicate that we don't handle "%p".
           */
        case 'p':
          if (sizeof(char *) <= sizeof(u_wide_int)) {
            ui_num = (u_wide_int)((size_t) va_arg(ap, char *));
            s = ap_php_conv_p2(ui_num, 4, 'x',
                &num_buf[NUM_BUF_SIZE], &s_len);
            if (ui_num != 0) {
              *--s = 'x';
              *--s = '0';
              s_len += 2;
            }
          } else {
            s = const_cast<char*>("%p");
            s_len = 2;
          }
          pad_char = ' ';
          break;


        case NUL:
          /*
           * The last character of the format string was %.
           * We ignore it.
           */
          continue;


fmt_error:
        throw Exception("Illegal length modifier specified '%c'", *fmt);

          /*
           * The default case is for unrecognized %'s.
           * We print %<char> to help the user identify what
           * option is not understood.
           * This is also useful in case the user wants to pass
           * the output of format_converter to another function
           * that understands some other %<char> (like syslog).
           * Note that we can't point s inside fmt because the
           * unknown <char> could be preceded by width etc.
           */
        default:
          char_buf[0] = '%';
          char_buf[1] = *fmt;
          s = char_buf;
          s_len = 2;
          pad_char = ' ';
          break;
      }

      if (prefix_char != NUL) {
        *--s = prefix_char;
        s_len++;
      }
      if (adjust_width && adjust == RIGHT && min_width > s_len) {
        if (pad_char == '0' && prefix_char != NUL) {
          appendchar(&result, &outpos, &size, *s);
          s++;
          s_len--;
          min_width--;
        }
        for (int i = 0; i < min_width - s_len; i++) {
          appendchar(&result, &outpos, &size, pad_char);
        }
      }
      /*
       * Print the (for now) non-null terminated string s.
       */
      appendsimplestring(&result, &outpos, &size, s, s_len);

      if (adjust_width && adjust == LEFT && min_width > s_len) {
        for (int i = 0; i < min_width - s_len; i++) {
          appendchar(&result, &outpos, &size, pad_char);
        }
      }
    }
skip_output:
    fmt++;
  }
  /*
   * Add the terminating null here since it wasn't added incrementally above
   * once the whole string has been composed.
   */
  result[outpos] = NUL;
  *outbuf = result;
  return outpos;
}