int CCITTFaxStream::lookChar() {
  int code1, code2, code3;
  int b1i, blackPixels, i, bits;
  GBool gotEOL;

  if (buf != EOF) {
    return buf;
  }

  // read the next row
  if (outputBits == 0) {

    // if at eof just return EOF
    if (eof) {
      return EOF;
    }

    err = gFalse;

    // 2-D encoding
    if (nextLine2D) {
      for (i = 0; i < columns && codingLine[i] < columns; ++i) {
	refLine[i] = codingLine[i];
      }
      refLine[i++] = columns;
      refLine[i] = columns;
      codingLine[0] = 0;
      a0i = 0;
      b1i = 0;
      blackPixels = 0;
      // invariant:
      // refLine[b1i-1] <= codingLine[a0i] < refLine[b1i] < refLine[b1i+1]
      //                                                             <= columns
      // exception at left edge:
      //   codingLine[a0i = 0] = refLine[b1i = 0] = 0 is possible
      // exception at right edge:
      //   refLine[b1i] = refLine[b1i+1] = columns is possible
      while (codingLine[a0i] < columns && !err) {
	code1 = getTwoDimCode();
	switch (code1) {
	case twoDimPass:
	  if (likely(b1i + 1 < columns + 2)) {
	    addPixels(refLine[b1i + 1], blackPixels);
	    if (refLine[b1i + 1] < columns) {
	      b1i += 2;
	    }
	  }
	  break;
	case twoDimHoriz:
	  code1 = code2 = 0;
	  if (blackPixels) {
	    do {
	      code1 += code3 = getBlackCode();
	    } while (code3 >= 64);
	    do {
	      code2 += code3 = getWhiteCode();
	    } while (code3 >= 64);
	  } else {
	    do {
	      code1 += code3 = getWhiteCode();
	    } while (code3 >= 64);
	    do {
	      code2 += code3 = getBlackCode();
	    } while (code3 >= 64);
	  }
	  addPixels(codingLine[a0i] + code1, blackPixels);
	  if (codingLine[a0i] < columns) {
	    addPixels(codingLine[a0i] + code2, blackPixels ^ 1);
	  }
	  while (refLine[b1i] <= codingLine[a0i] && refLine[b1i] < columns) {
	    b1i += 2;
	    if (unlikely(b1i > columns + 1)) {
	      error(errSyntaxError, getPos(),
		"Bad 2D code {0:04x} in CCITTFax stream", code1);
	      err = gTrue;
	      break;
	    }
	  }
	  break;
	case twoDimVertR3:
	  if (unlikely(b1i > columns + 1)) {
	    error(errSyntaxError, getPos(),
	      "Bad 2D code {0:04x} in CCITTFax stream", code1);
	    err = gTrue;
	    break;
	  }
	  addPixels(refLine[b1i] + 3, blackPixels);
	  blackPixels ^= 1;
	  if (codingLine[a0i] < columns) {
	    ++b1i;
	    while (refLine[b1i] <= codingLine[a0i] && refLine[b1i] < columns) {
	      b1i += 2;
	      if (unlikely(b1i > columns + 1)) {
		error(errSyntaxError, getPos(),
		  "Bad 2D code {0:04x} in CCITTFax stream", code1);
		err = gTrue;
		break;
	      }
	    }
	  }
	  break;
	case twoDimVertR2:
	  if (unlikely(b1i > columns + 1)) {
	    error(errSyntaxError, getPos(),
	      "Bad 2D code {0:04x} in CCITTFax stream", code1);
	    err = gTrue;
	    break;
	  }
	  addPixels(refLine[b1i] + 2, blackPixels);
	  blackPixels ^= 1;
	  if (codingLine[a0i] < columns) {
	    ++b1i;
	    while (refLine[b1i] <= codingLine[a0i] && refLine[b1i] < columns) {
	      b1i += 2;
	      if (unlikely(b1i > columns + 1)) {
		error(errSyntaxError, getPos(),
		  "Bad 2D code {0:04x} in CCITTFax stream", code1);
		err = gTrue;
		break;
	      }
	    }
	  }
	  break;
	case twoDimVertR1:
	  if (unlikely(b1i > columns + 1)) {
	    error(errSyntaxError, getPos(),
	      "Bad 2D code {0:04x} in CCITTFax stream", code1);
	    err = gTrue;
	    break;
	  }
	  addPixels(refLine[b1i] + 1, blackPixels);
	  blackPixels ^= 1;
	  if (codingLine[a0i] < columns) {
	    ++b1i;
	    while (refLine[b1i] <= codingLine[a0i] && refLine[b1i] < columns) {
	      b1i += 2;
	      if (unlikely(b1i > columns + 1)) {
		error(errSyntaxError, getPos(),
		  "Bad 2D code {0:04x} in CCITTFax stream", code1);
		err = gTrue;
		break;
	      }
	    }
	  }
	  break;
	case twoDimVert0:
	  if (unlikely(b1i > columns + 1)) {
	    error(errSyntaxError, getPos(),
	      "Bad 2D code {0:04x} in CCITTFax stream", code1);
	    err = gTrue;
	    break;
	  }
	  addPixels(refLine[b1i], blackPixels);
	  blackPixels ^= 1;
	  if (codingLine[a0i] < columns) {
	    ++b1i;
	    while (refLine[b1i] <= codingLine[a0i] && refLine[b1i] < columns) {
	      b1i += 2;
	      if (unlikely(b1i > columns + 1)) {
		error(errSyntaxError, getPos(),
		  "Bad 2D code {0:04x} in CCITTFax stream", code1);
		err = gTrue;
		break;
	      }
	    }
	  }
	  break;
	case twoDimVertL3:
	  if (unlikely(b1i > columns + 1)) {
	    error(errSyntaxError, getPos(),
	      "Bad 2D code {0:04x} in CCITTFax stream", code1);
	    err = gTrue;
	    break;
	  }
	  addPixelsNeg(refLine[b1i] - 3, blackPixels);
	  blackPixels ^= 1;
	  if (codingLine[a0i] < columns) {
	    if (b1i > 0) {
	      --b1i;
	    } else {
	      ++b1i;
	    }
	    while (refLine[b1i] <= codingLine[a0i] && refLine[b1i] < columns) {
	      b1i += 2;
	      if (unlikely(b1i > columns + 1)) {
		error(errSyntaxError, getPos(),
		  "Bad 2D code {0:04x} in CCITTFax stream", code1);
		err = gTrue;
		break;
	      }
	    }
	  }
	  break;
	case twoDimVertL2:
	  if (unlikely(b1i > columns + 1)) {
	    error(errSyntaxError, getPos(),
	      "Bad 2D code {0:04x} in CCITTFax stream", code1);
	    err = gTrue;
	    break;
	  }
	  addPixelsNeg(refLine[b1i] - 2, blackPixels);
	  blackPixels ^= 1;
	  if (codingLine[a0i] < columns) {
	    if (b1i > 0) {
	      --b1i;
	    } else {
	      ++b1i;
	    }
	    while (refLine[b1i] <= codingLine[a0i] && refLine[b1i] < columns) {
	      b1i += 2;
	      if (unlikely(b1i > columns + 1)) {
	        error(errSyntaxError, getPos(),
		  "Bad 2D code {0:04x} in CCITTFax stream", code1);
	        err = gTrue;
	        break;
	      }
	    }
	  }
	  break;
	case twoDimVertL1:
	  if (unlikely(b1i > columns + 1)) {
	    error(errSyntaxError, getPos(),
	      "Bad 2D code {0:04x} in CCITTFax stream", code1);
	    err = gTrue;
	    break;
	  }
	  addPixelsNeg(refLine[b1i] - 1, blackPixels);
	  blackPixels ^= 1;
	  if (codingLine[a0i] < columns) {
	    if (b1i > 0) {
	      --b1i;
	    } else {
	      ++b1i;
	    }
	    while (refLine[b1i] <= codingLine[a0i] && refLine[b1i] < columns) {
	      b1i += 2;
	      if (unlikely(b1i > columns + 1)) {
		error(errSyntaxError, getPos(),
		  "Bad 2D code {0:04x} in CCITTFax stream", code1);
		err = gTrue;
		break;
	      }
	    }
	  }
	  break;
	case EOF:
	  addPixels(columns, 0);
	  eof = gTrue;
	  break;
	default:
	  error(errSyntaxError, getPos(),
		"Bad 2D code {0:04x} in CCITTFax stream", code1);
	  addPixels(columns, 0);
	  err = gTrue;
	  break;
	}
      }

    // 1-D encoding
    } else {
      codingLine[0] = 0;
      a0i = 0;
      blackPixels = 0;
      while (codingLine[a0i] < columns) {
	code1 = 0;
	if (blackPixels) {
	  do {
	    code1 += code3 = getBlackCode();
	  } while (code3 >= 64);
	} else {
	  do {
	    code1 += code3 = getWhiteCode();
	  } while (code3 >= 64);
	}
	addPixels(codingLine[a0i] + code1, blackPixels);
	blackPixels ^= 1;
      }
    }

    // check for end-of-line marker, skipping over any extra zero bits
    // (if EncodedByteAlign is true and EndOfLine is false, there can
    // be "false" EOL markers -- i.e., if the last n unused bits in
    // row i are set to zero, and the first 11-n bits in row i+1
    // happen to be zero -- so we don't look for EOL markers in this
    // case)
    gotEOL = gFalse;
    if (!endOfBlock && row == rows - 1) {
      eof = gTrue;
    } else if (endOfLine || !byteAlign) {
      code1 = lookBits(12);
      if (endOfLine) {
	while (code1 != EOF && code1 != 0x001) {
	  eatBits(1);
	  code1 = lookBits(12);
	}
      } else {
	while (code1 == 0) {
	  eatBits(1);
	  code1 = lookBits(12);
	}
      }
      if (code1 == 0x001) {
	eatBits(12);
	gotEOL = gTrue;
      }
    }

    // byte-align the row
    // (Adobe apparently doesn't do byte alignment after EOL markers
    // -- I've seen CCITT image data streams in two different formats,
    // both with the byteAlign flag set:
    //   1. xx:x0:01:yy:yy
    //   2. xx:00:1y:yy:yy
    // where xx is the previous line, yy is the next line, and colons
    // separate bytes.)
    if (byteAlign && !gotEOL) {
      inputBits &= ~7;
    }

    // check for end of stream
    if (lookBits(1) == EOF) {
      eof = gTrue;
    }

    // get 2D encoding tag
    if (!eof && encoding > 0) {
      nextLine2D = !lookBits(1);
      eatBits(1);
    }

    // check for end-of-block marker
    if (endOfBlock && !endOfLine && byteAlign) {
      // in this case, we didn't check for an EOL code above, so we
      // need to check here
      code1 = lookBits(24);
      if (code1 == 0x001001) {
	eatBits(12);
	gotEOL = gTrue;
      }
    }
    if (endOfBlock && gotEOL) {
      code1 = lookBits(12);
      if (code1 == 0x001) {
	eatBits(12);
	if (encoding > 0) {
	  lookBits(1);
	  eatBits(1);
	}
	if (encoding >= 0) {
	  for (i = 0; i < 4; ++i) {
	    code1 = lookBits(12);
	    if (code1 != 0x001) {
	      error(errSyntaxError, getPos(),
		    "Bad RTC code in CCITTFax stream");
	    }
	    eatBits(12);
	    if (encoding > 0) {
	      lookBits(1);
	      eatBits(1);
	    }
	  }
	}
	eof = gTrue;
      }

    // look for an end-of-line marker after an error -- we only do
    // this if we know the stream contains end-of-line markers because
    // the "just plow on" technique tends to work better otherwise
    } else if (err && endOfLine) {
      while (1) {
	code1 = lookBits(13);
	if (code1 == EOF) {
	  eof = gTrue;
	  return EOF;
	}
	if ((code1 >> 1) == 0x001) {
	  break;
	}
	eatBits(1);
      }
      eatBits(12); 
      if (encoding > 0) {
	eatBits(1);
	nextLine2D = !(code1 & 1);
      }
    }

    // set up for output
    if (codingLine[0] > 0) {
      outputBits = codingLine[a0i = 0];
    } else {
      outputBits = codingLine[a0i = 1];
    }

    ++row;
  }

  // get a byte
  if (outputBits >= 8) {
    buf = (a0i & 1) ? 0x00 : 0xff;
    outputBits -= 8;
    if (outputBits == 0 && codingLine[a0i] < columns) {
      ++a0i;
      outputBits = codingLine[a0i] - codingLine[a0i - 1];
    }
  } else {
    bits = 8;
    buf = 0;
    do {
      if (outputBits > bits) {
	buf <<= bits;
	if (!(a0i & 1)) {
	  buf |= 0xff >> (8 - bits);
	}
	outputBits -= bits;
	bits = 0;
      } else {
	buf <<= outputBits;
	if (!(a0i & 1)) {
	  buf |= 0xff >> (8 - outputBits);
	}
	bits -= outputBits;
	outputBits = 0;
	if (codingLine[a0i] < columns) {
	  ++a0i;
	  if (unlikely(a0i > columns)) {
	    error(errSyntaxError, getPos(),
	      "Bad bits {0:04x} in CCITTFax stream", bits);
	      err = gTrue;
	      break;
	  }
	  outputBits = codingLine[a0i] - codingLine[a0i - 1];
	} else if (bits > 0) {
	  buf <<= bits;
	  bits = 0;
	}
      }
    } while (bits);
  }
  if (black) {
    buf ^= 0xff;
  }
  return buf;
}