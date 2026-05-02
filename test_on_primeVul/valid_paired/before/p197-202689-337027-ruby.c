rb_str_justify(int argc, VALUE *argv, VALUE str, char jflag)
{
    rb_encoding *enc;
    VALUE w;
    long width, len, flen = 1, fclen = 1;
    VALUE res;
    char *p;
    const char *f = " ";
    long n, llen, rlen;
    volatile VALUE pad;
    int singlebyte = 1, cr;

    rb_scan_args(argc, argv, "11", &w, &pad);
    enc = STR_ENC_GET(str);
    width = NUM2LONG(w);
    if (argc == 2) {
	StringValue(pad);
	enc = rb_enc_check(str, pad);
	f = RSTRING_PTR(pad);
	flen = RSTRING_LEN(pad);
	fclen = str_strlen(pad, enc);
	singlebyte = single_byte_optimizable(pad);
	if (flen == 0 || fclen == 0) {
	    rb_raise(rb_eArgError, "zero width padding");
	}
    }
    len = str_strlen(str, enc);
    if (width < 0 || len >= width) return rb_str_dup(str);
    n = width - len;
    llen = (jflag == 'l') ? 0 : ((jflag == 'r') ? n : n/2);
    rlen = n - llen;
    cr = ENC_CODERANGE(str);
    res = rb_str_new5(str, 0, RSTRING_LEN(str)+n*flen/fclen+2);
    p = RSTRING_PTR(res);
    while (llen) {
	if (flen <= 1) {
	    *p++ = *f;
	    llen--;
	}
	else if (llen > fclen) {
	    memcpy(p,f,flen);
	    p += flen;
	    llen -= fclen;
	}
	else {
	    char *fp = str_nth(f, f+flen, llen, enc, singlebyte);
	    n = fp - f;
	    memcpy(p,f,n);
	    p+=n;
	    break;
	}
    }
    memcpy(p, RSTRING_PTR(str), RSTRING_LEN(str));
    p+=RSTRING_LEN(str);
    while (rlen) {
	if (flen <= 1) {
	    *p++ = *f;
	    rlen--;
	}
	else if (rlen > fclen) {
	    memcpy(p,f,flen);
	    p += flen;
	    rlen -= fclen;
	}
	else {
	    char *fp = str_nth(f, f+flen, rlen, enc, singlebyte);
	    n = fp - f;
	    memcpy(p,f,n);
	    p+=n;
	    break;
	}
    }
    *p = '\0';
    STR_SET_LEN(res, p-RSTRING_PTR(res));
    OBJ_INFECT(res, str);
    if (!NIL_P(pad)) OBJ_INFECT(res, pad);
    rb_enc_associate(res, enc);
    if (argc == 2)
	cr = ENC_CODERANGE_AND(cr, ENC_CODERANGE(pad));
    if (cr != ENC_CODERANGE_BROKEN)
	ENC_CODERANGE_SET(res, cr);
    return res;
}