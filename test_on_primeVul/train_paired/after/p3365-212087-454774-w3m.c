wc_push_to_iso2022(Str os, wc_wchar_t cc, wc_status *st)
{
    wc_uchar g = 0;
    wc_bool is_wide = WC_FALSE, retry = WC_FALSE;
    wc_wchar_t cc2;

  while (1) {
    switch (WC_CCS_TYPE(cc.ccs)) {
    case WC_CCS_A_CS94:
	if (cc.ccs == WC_CCS_US_ASCII)
	    cc.ccs = st->g0_ccs;
	if (WC_CCS_INDEX(cc.ccs) >= WC_F_ISO_BASE)
	    g = cs94_gmap[WC_CCS_INDEX(cc.ccs) - WC_F_ISO_BASE];
	break;
    case WC_CCS_A_CS94W:
	is_wide = 1;
	switch (cc.ccs) {
#ifdef USE_UNICODE
	case WC_CCS_JIS_X_0212:
	    if (!WcOption.use_jisx0212 && WcOption.use_jisx0213 &&
		WcOption.ucs_conv) {
		cc2 = wc_jisx0212_to_jisx0213(cc);
		if (cc2.ccs == WC_CCS_JIS_X_0213_1 ||
		    cc2.ccs == WC_CCS_JIS_X_0213_2) {
		    cc = cc2;
		    continue;
		}
	    }
	    break;
	case WC_CCS_JIS_X_0213_1:
	case WC_CCS_JIS_X_0213_2:
	    if (!WcOption.use_jisx0213 && WcOption.use_jisx0212 &&
		WcOption.ucs_conv) {
		cc2 = wc_jisx0213_to_jisx0212(cc);
		if (cc2.ccs == WC_CCS_JIS_X_0212) {
		    cc = cc2;
		    continue;
		}
	    }
	    break;
#endif
	}
	if (WC_CCS_INDEX(cc.ccs) >= WC_F_ISO_BASE)
	    g = cs94w_gmap[WC_CCS_INDEX(cc.ccs) - WC_F_ISO_BASE];
	break;
    case WC_CCS_A_CS96:
	if (WC_CCS_INDEX(cc.ccs) >= WC_F_ISO_BASE)
	    g = cs96_gmap[WC_CCS_INDEX(cc.ccs) - WC_F_ISO_BASE];
	break;
    case WC_CCS_A_CS96W:
	is_wide = 1;
	if (WC_CCS_INDEX(cc.ccs) >= WC_F_ISO_BASE)
	    g = cs96w_gmap[WC_CCS_INDEX(cc.ccs) - WC_F_ISO_BASE];
	break;
    case WC_CCS_A_CS942:
	if (WC_CCS_INDEX(cc.ccs) >= WC_F_ISO_BASE)
	    g = cs942_gmap[WC_CCS_INDEX(cc.ccs) - WC_F_ISO_BASE];
	break;
    case WC_CCS_A_UNKNOWN_W:
	if (WcOption.no_replace)
	    return;
	is_wide = 1;
	cc.ccs = WC_CCS_US_ASCII;
	if (WC_CCS_INDEX(cc.ccs) >= WC_F_ISO_BASE)
	    g = cs94_gmap[WC_CCS_INDEX(cc.ccs) - WC_F_ISO_BASE];
	cc.code = ((wc_uint32)WC_REPLACE_W[0] << 8) | WC_REPLACE_W[1];
	break;
    case WC_CCS_A_UNKNOWN:
	if (WcOption.no_replace)
	    return;
	cc.ccs = WC_CCS_US_ASCII;
	if (WC_CCS_INDEX(cc.ccs) >= WC_F_ISO_BASE)
	    g = cs94_gmap[WC_CCS_INDEX(cc.ccs) - WC_F_ISO_BASE];
	cc.code = (wc_uint32)WC_REPLACE[0];
	break;
    default:
	if ((cc.ccs == WC_CCS_JOHAB || cc.ccs == WC_CCS_JOHAB_1 ||
		cc.ccs == WC_CCS_JOHAB_2 || cc.ccs == WC_CCS_JOHAB_3) &&
		cs94w_gmap[WC_F_KS_X_1001 - WC_F_ISO_BASE]) {
	    wc_wchar_t cc2 = wc_johab_to_ksx1001(cc);
	    if (cc2.ccs == WC_CCS_KS_X_1001) {
		cc = cc2;
		continue;
	    }
	}
#ifdef USE_UNICODE
	if (WcOption.ucs_conv)
	    cc = wc_any_to_iso2022(cc, st);
	else
#endif
	    cc.ccs = WC_CCS_IS_WIDE(cc.ccs) ? WC_CCS_UNKNOWN_W : WC_CCS_UNKNOWN;
	continue;
    }
    if (! g) {
#ifdef USE_UNICODE
	if (WcOption.ucs_conv && ! retry)
	    cc = wc_any_to_any_ces(cc, st);
	else
#endif
	    cc.ccs = WC_CCS_IS_WIDE(cc.ccs) ? WC_CCS_UNKNOWN_W : WC_CCS_UNKNOWN;
	retry = WC_TRUE;
	continue;
    }

    wc_push_iso2022_esc(os, cc.ccs, g, 1, st);
    if (is_wide)
	Strcat_char(os, (char)((cc.code >> 8) & 0x7f));
    Strcat_char(os, (char)(cc.code & 0x7f));
    return;
  }
}