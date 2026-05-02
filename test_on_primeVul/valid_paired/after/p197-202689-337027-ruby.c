rb_str_justify(int argc, VALUE *argv, VALUE str, char jflag)
{
    rb_encoding *enc;
    VALUE w;
    long width, len, flen = 1, fclen = 1;
    VALUE res;
    char *p;
    const char *f = " ";
    long n, size, llen, rlen, llen2 = 0, rlen2 = 0;
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
    if (flen > 1) {
       llen2 = str_offset(f, f + flen, llen % fclen, enc, singlebyte);
       rlen2 = str_offset(f, f + flen, rlen % fclen, enc, singlebyte);
    }
    size = RSTRING_LEN(str);
    if ((len = llen / fclen + rlen / fclen) >= LONG_MAX / flen ||
       (len *= flen) >= LONG_MAX - llen2 - rlen2 ||
       (len += llen2 + rlen2) >= LONG_MAX - size) {
       rb_raise(rb_eArgError, "argument too big");
    }
    len += size;
    res = rb_str_new5(str, 0, len);
    p = RSTRING_PTR(res);
    if (flen <= 1) {
       memset(p, *f, llen);
       p += llen;
    }
    else {
       while (llen > fclen) {
	    memcpy(p,f,flen);
	    p += flen;
	    llen -= fclen;
	}
       if (llen > 0) {
           memcpy(p, f, llen2);
           p += llen2;
	}
    }
    memcpy(p, RSTRING_PTR(str), size);
    p += size;
    if (flen <= 1) {
       memset(p, *f, rlen);
       p += rlen;
    }
    else {
       while (rlen > fclen) {
	    memcpy(p,f,flen);
	    p += flen;
	    rlen -= fclen;
	}
       if (rlen > 0) {
           memcpy(p, f, rlen2);
           p += rlen2;
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