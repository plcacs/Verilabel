wc_any_to_ucs(wc_wchar_t cc)
{
    int f;
    wc_uint16 *map = NULL;
    wc_uint32 map_size = 0x80;
    wc_map *map2;

    f = WC_CCS_INDEX(cc.ccs);
    switch (WC_CCS_TYPE(cc.ccs)) {
    case WC_CCS_A_CS94:
	if (cc.ccs == WC_CCS_US_ASCII)
	    return cc.code;
	if (f < WC_F_ISO_BASE || f > WC_F_CS94_END)
	    return WC_C_UCS4_ERROR;
	map = cs94_ucs_map[f - WC_F_ISO_BASE];
	cc.code &= 0x7f;
	break;
    case WC_CCS_A_CS94W:
	if (cc.ccs == WC_CCS_GB_2312 && WcOption.use_gb12345_map) {
	    cc.ccs = WC_CCS_GB_12345;
	    return wc_any_to_ucs(cc);
	} else if (cc.ccs == WC_CCS_JIS_X_0213_1) {
	    map2 = wc_map_search((wc_uint16)(cc.code & 0x7f7f),
		jisx02131_ucs_p2_map, N_jisx02131_ucs_p2_map);
	    if (map2)
		return map2->code2 | WC_C_UCS4_PLANE2;
	} else if (cc.ccs == WC_CCS_JIS_X_0213_2) {
	    map2 = wc_map_search((wc_uint16)(cc.code & 0x7f7f),
		jisx02132_ucs_p2_map, N_jisx02132_ucs_p2_map);
	    if (map2)
		return map2->code2 | WC_C_UCS4_PLANE2;
	}
	if (f < WC_F_ISO_BASE || f > WC_F_CS94W_END)
	    return 0;
	map = cs94w_ucs_map[f - WC_F_ISO_BASE];
	map_size = cs94w_ucs_map_size[f - WC_F_ISO_BASE];
	cc.code = WC_CS94W_N(cc.code);
	break;
    case WC_CCS_A_CS96:
	if (f < WC_F_ISO_BASE || f > WC_F_CS96_END)
	    return WC_C_UCS4_ERROR;
	map = cs96_ucs_map[f - WC_F_ISO_BASE];
	cc.code &= 0x7f;
	break;
    case WC_CCS_A_CS96W:
	if (f < WC_F_ISO_BASE || f > WC_F_CS96W_END)
	    return WC_C_UCS4_ERROR;
	map = cs96w_ucs_map[f - WC_F_ISO_BASE];
	map_size = cs96w_ucs_map_size[f - WC_F_ISO_BASE];
	cc.code = WC_CS96W_N(cc.code);
	break;
    case WC_CCS_A_CS942:
	if (f < WC_F_ISO_BASE || f > WC_F_CS942_END)
	    return WC_C_UCS4_ERROR;
	map = cs942_ucs_map[f - WC_F_ISO_BASE];
	cc.code &= 0x7f;
	break;
    case WC_CCS_A_PCS:
	if (f < WC_F_PCS_BASE || f > WC_F_PCS_END)
	    return WC_C_UCS4_ERROR;
	switch (cc.ccs) {
	case WC_CCS_CP1258_2:
	    map2 = wc_map_search((wc_uint16)cc.code,
		cp12582_ucs_map, N_cp12582_ucs_map);
	    if (map2)
		return map2->code2;
	    return WC_C_UCS4_ERROR;
	case WC_CCS_TCVN_5712_3:
	    return wc_any_to_ucs(wc_tcvn57123_to_tcvn5712(cc));
	case WC_CCS_GBK_80:
	    return WC_C_UCS2_EURO;
	}
	map = pcs_ucs_map[f - WC_F_PCS_BASE];
	map_size = pcs_ucs_map_size[f - WC_F_PCS_BASE];
	cc.code &= 0x7f;
	break;
    case WC_CCS_A_PCSW:
	if (f < WC_F_PCS_BASE || f > WC_F_PCSW_END)
	    return WC_C_UCS4_ERROR;
	map = pcsw_ucs_map[f - WC_F_PCS_BASE];
	map_size = pcsw_ucs_map_size[f - WC_F_PCS_BASE];
	switch (cc.ccs) {
	case WC_CCS_BIG5:
	    cc.code = WC_BIG5_N(cc.code);
	    break;
	case WC_CCS_BIG5_2:
	    cc.code = WC_CS94W_N(cc.code) + WC_C_BIG5_2_BASE;
	    break;
	case WC_CCS_HKSCS_1:
	case WC_CCS_HKSCS_2:
	    cc = wc_cs128w_to_hkscs(cc);
	case WC_CCS_HKSCS:
	    map2 = wc_map_search((wc_uint16)cc.code,
		hkscs_ucs_p2_map, N_hkscs_ucs_p2_map);
	    if (map2)
		return map2->code2 | WC_C_UCS4_PLANE2;
	    cc.code = wc_hkscs_to_N(cc.code);
	    break;
	case WC_CCS_JOHAB:
	    return wc_any_to_ucs(wc_johab_to_cs128w(cc));
	case WC_CCS_JOHAB_1:
	    return WC_CS94x128_N(cc.code) + WC_C_UCS2_HANGUL;
	case WC_CCS_JOHAB_2:
	    cc.code = WC_CS128W_N(cc.code);
	    cc.code = WC_N_JOHAB2(cc.code);
	    map2 = wc_map_search((wc_uint16)cc.code,
		johab2_ucs_map, N_johab2_ucs_map);
	    if (map2)
		return map2->code2;
	    return WC_C_UCS4_ERROR;
	case WC_CCS_JOHAB_3:
	    if ((cc.code & 0x7f7f) < 0x2121)
		return WC_C_UCS4_ERROR;
	case WC_CCS_SJIS_EXT:
	    return wc_any_to_ucs(wc_sjis_ext_to_cs94w(cc));
	case WC_CCS_SJIS_EXT_1:
	    cc.code = wc_sjis_ext1_to_N(cc.code);
	    if (cc.code == WC_C_SJIS_ERROR)
		return WC_C_UCS4_ERROR;
	    break;
	case WC_CCS_SJIS_EXT_2:
	    cc.code = wc_sjis_ext2_to_N(cc.code);
	    if (cc.code == WC_C_SJIS_ERROR)
		return WC_C_UCS4_ERROR;
	    break;
	case WC_CCS_GBK_1:
	case WC_CCS_GBK_2:
	    cc = wc_cs128w_to_gbk(cc);
	case WC_CCS_GBK:
	    cc.code = wc_gbk_to_N(cc.code);
	    break;
	case WC_CCS_GBK_EXT:
	case WC_CCS_GBK_EXT_1:
	case WC_CCS_GBK_EXT_2:
	    return wc_gb18030_to_ucs(cc);
	case WC_CCS_UHC_1:
	case WC_CCS_UHC_2:
	    cc = wc_cs128w_to_uhc(cc);
	case WC_CCS_UHC:
	    if (cc.code > WC_C_UHC_END)
		return WC_C_UCS4_ERROR;
	    cc.code = wc_uhc_to_N(cc.code);
	    break;
	default:
	    cc.code = WC_CS94W_N(cc.code);
	    break;
	}
	break;
    case WC_CCS_A_WCS16:
	switch (WC_CCS_SET(cc.ccs)) {
	case WC_CCS_UCS2:
	    return cc.code;
	}
	return WC_C_UCS4_ERROR;
    case WC_CCS_A_WCS32:
	switch (WC_CCS_SET(cc.ccs)) {
	case WC_CCS_UCS4:
	    return cc.code;
	case WC_CCS_UCS_TAG:
	    return wc_ucs_tag_to_ucs(cc.code);
	case WC_CCS_GB18030:
	    return wc_gb18030_to_ucs(cc);
	}
	return WC_C_UCS4_ERROR;
    case WC_CCS_A_UNKNOWN:
	if (cc.ccs == WC_CCS_C1)
	    return (cc.code | 0x80);
    default:
	return WC_C_UCS4_ERROR;
    }
    if (map == NULL)
	return WC_C_UCS4_ERROR;
    if (map_size == 0 || cc.code > map_size - 1)
	return WC_C_UCS4_ERROR;
    cc.code = map[cc.code];
    return cc.code ? cc.code : WC_C_UCS4_ERROR;
}