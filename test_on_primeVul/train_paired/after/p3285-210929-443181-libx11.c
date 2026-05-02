init_om(
    XOM om)
{
    XLCd lcd = om->core.lcd;
    XOMGenericPart *gen = XOM_GENERIC(om);
    OMData data;
    XlcCharSet *charset_list;
    FontData font_data;
    char **required_list;
    XOrientation *orientation;
    char **value, buf[BUFSIZ], *bufptr;
    int count = 0, num = 0;
    unsigned int length = 0;

    _XlcGetResource(lcd, "XLC_FONTSET", "on_demand_loading", &value, &count);
    if (count > 0 && _XlcCompareISOLatin1(*value, "True") == 0)
	gen->on_demand_loading = True;

    _XlcGetResource(lcd, "XLC_FONTSET", "object_name", &value, &count);
    if (count > 0) {
	gen->object_name = strdup(*value);
	if (gen->object_name == NULL)
	    return False;
    }

    for (num = 0; ; num++) {

        snprintf(buf, sizeof(buf), "fs%d.charset.name", num);
        _XlcGetResource(lcd, "XLC_FONTSET", buf, &value, &count);

        if( count < 1){
            snprintf(buf, sizeof(buf), "fs%d.charset", num);
            _XlcGetResource(lcd, "XLC_FONTSET", buf, &value, &count);
            if (count < 1)
                break;
        }

	data = add_data(om);
	if (data == NULL)
	    return False;

	charset_list = Xmalloc(sizeof(XlcCharSet) * count);
	if (charset_list == NULL)
	    return False;
	data->charset_list = charset_list;
	data->charset_count = count;

	while (count-- > 0){
	    *charset_list++ = _XlcGetCharSet(*value++);
        }
        snprintf(buf, sizeof(buf), "fs%d.charset.udc_area", num);
        _XlcGetResource(lcd, "XLC_FONTSET", buf, &value, &count);
        if( count > 0){
            UDCArea udc;
            int i,flag = 0;
            udc = Xmalloc(count * sizeof(UDCAreaRec));
	    if (udc == NULL)
	        return False;
            for(i=0;i<count;i++){
                sscanf(value[i],"\\x%lx,\\x%lx", &(udc[i].start),
		       &(udc[i].end));
            }
            for(i=0;i<data->charset_count;i++){
		if(data->charset_list[i]->udc_area == NULL){
		    data->charset_list[i]->udc_area     = udc;
		    data->charset_list[i]->udc_area_num = count;
		    flag = 1;
		}
            }
	    if(flag == 0){
		Xfree(udc);
	    }
        }

        snprintf(buf, sizeof(buf), "fs%d.font.primary", num);
        _XlcGetResource(lcd, "XLC_FONTSET", buf, &value, &count);
        if (count < 1){
            snprintf(buf, sizeof(buf), "fs%d.font", num);
            _XlcGetResource(lcd, "XLC_FONTSET", buf, &value, &count);
            if (count < 1)
                return False;
        }

	font_data = read_EncodingInfo(count,value);
	if (font_data == NULL)
	    return False;

	data->font_data = font_data;
	data->font_data_count = count;

        snprintf(buf, sizeof(buf), "fs%d.font.substitute", num);
        _XlcGetResource(lcd, "XLC_FONTSET", buf, &value, &count);
        if (count > 0){
            font_data = read_EncodingInfo(count,value);
            if (font_data == NULL)
	        return False;
            data->substitute      = font_data;
            data->substitute_num = count;
        } else {
            snprintf(buf, sizeof(buf), "fs%d.font", num);
            _XlcGetResource(lcd, "XLC_FONTSET", buf, &value, &count);
            if (count < 1) {
                data->substitute      = NULL;
                data->substitute_num = 0;
	    } else {
                font_data = read_EncodingInfo(count,value);
                data->substitute      = font_data;
                data->substitute_num = count;
	    }
	}
        read_vw(lcd,data,num);
	length += strlen(data->font_data->name) + 1;
    }

    /* required charset list */
    required_list = Xmalloc(sizeof(char *) * gen->data_num);
    if (required_list == NULL)
	return False;

    om->core.required_charset.charset_list = required_list;
    om->core.required_charset.charset_count = gen->data_num;

    count = gen->data_num;
    data = gen->data;

    if (count > 0) {
	bufptr = Xmalloc(length);
	if (bufptr == NULL) {
	    Xfree(required_list);
	    return False;
	}

	for ( ; count-- > 0; data++) {
	    strcpy(bufptr, data->font_data->name);
	    *required_list++ = bufptr;
	    bufptr += strlen(bufptr) + 1;
	}
    }

    /* orientation list */
    orientation = Xmalloc(sizeof(XOrientation) * 2);
    if (orientation == NULL)
	return False;

    orientation[0] = XOMOrientation_LTR_TTB;
    orientation[1] = XOMOrientation_TTB_RTL;
    om->core.orientation_list.orientation = orientation;
    om->core.orientation_list.num_orientation = 2;

    /* directional dependent drawing */
    om->core.directional_dependent = False;

    /* contextual drawing */
    om->core.contextual_drawing = False;

    /* context dependent */
    om->core.context_dependent = False;

    return True;
}