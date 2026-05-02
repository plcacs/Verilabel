parseChars(FileInfo *nested, CharsString *result, CharsString *token) {
	int in = 0;
	int out = 0;
	int lastOutSize = 0;
	int lastIn;
	unsigned int ch = 0;
	int numBytes = 0;
	unsigned int utf32 = 0;
	int k;
	while (in < token->length) {
		ch = token->chars[in++] & 0xff;
		if (ch < 128) {
			if (ch == '\\') { /* escape sequence */
				switch (ch = token->chars[in]) {
				case '\\':
					break;
				case 'e':
					ch = 0x1b;
					break;
				case 'f':
					ch = 12;
					break;
				case 'n':
					ch = 10;
					break;
				case 'r':
					ch = 13;
					break;
				case 's':
					ch = ' ';
					break;
				case 't':
					ch = 9;
					break;
				case 'v':
					ch = 11;
					break;
				case 'w':
					ch = ENDSEGMENT;
					break;
				case 34:
					ch = QUOTESUB;
					break;
				case 'X':
				case 'x':
					if (token->length - in > 4) {
						ch = hexValue(nested, &token->chars[in + 1], 4);
						in += 4;
					}
					break;
				case 'y':
				case 'Y':
					if (CHARSIZE == 2) {
					not32:
						compileError(nested,
								"liblouis has not been compiled for 32-bit Unicode");
						break;
					}
					if (token->length - in > 5) {
						ch = hexValue(nested, &token->chars[in + 1], 5);
						in += 5;
					}
					break;
				case 'z':
				case 'Z':
					if (CHARSIZE == 2) goto not32;
					if (token->length - in > 8) {
						ch = hexValue(nested, &token->chars[in + 1], 8);
						in += 8;
					}
					break;
				default:
					compileError(nested, "invalid escape sequence '\\%c'", ch);
					break;
				}
				in++;
			}
			result->chars[out++] = (widechar)ch;
			if (out >= MAXSTRING) {
				result->length = out;
				return 1;
			}
			continue;
		}
		lastOutSize = out;
		lastIn = in;
		for (numBytes = MAXBYTES - 1; numBytes > 0; numBytes--)
			if (ch >= first0Bit[numBytes]) break;
		utf32 = ch & (0XFF - first0Bit[numBytes]);
		for (k = 0; k < numBytes; k++) {
			if (in >= MAXSTRING) break;
			if (out >= MAXSTRING) {
				result->length = lastOutSize;
				return 1;
			}
			if (token->chars[in] < 128 || (token->chars[in] & 0x0040)) {
				compileWarning(nested, "invalid UTF-8. Assuming Latin-1.");
				result->chars[out++] = token->chars[lastIn];
				in = lastIn + 1;
				continue;
			}
			utf32 = (utf32 << 6) + (token->chars[in++] & 0x3f);
		}
		if (out >= MAXSTRING) {
			result->length = lastOutSize;
			return 1;
		}
		if (CHARSIZE == 2 && utf32 > 0xffff) utf32 = 0xffff;
		result->chars[out++] = (widechar)utf32;
	}
	result->length = out;
	return 1;
}