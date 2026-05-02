_XimAttributeToValue(
    Xic			  ic,
    XIMResourceList	  res,
    CARD16		 *data,
    CARD16		  data_len,
    XPointer		  value,
    BITMASK32		  mode)
{
    switch (res->resource_size) {
    case XimType_SeparatorOfNestedList:
    case XimType_NEST:
	break;

    case XimType_CARD8:
    case XimType_CARD16:
    case XimType_CARD32:
    case XimType_Window:
    case XimType_XIMHotKeyState:
	_XCopyToArg((XPointer)data, (XPointer *)&value, data_len);
	break;

    case XimType_STRING8:
	{
	    char	*str;

	    if (!(value))
		return False;

	    if (!(str = Xmalloc(data_len + 1)))
		return False;

	    (void)memcpy(str, (char *)data, data_len);
	    str[data_len] = '\0';

	    *((char **)value) = str;
	    break;
	}

    case XimType_XIMStyles:
	{
	    CARD16		 num = data[0];
	    register CARD32	*style_list = (CARD32 *)&data[2];
	    XIMStyle		*style;
	    XIMStyles		*rep;
	    register int	 i;
	    char		*p;
	    unsigned int         alloc_len;

	    if (!(value))
		return False;

	    if (num > (USHRT_MAX / sizeof(XIMStyle)))
		return False;
	    if ((sizeof(num) + (num * sizeof(XIMStyle))) > data_len)
		return False;
	    alloc_len = sizeof(XIMStyles) + sizeof(XIMStyle) * num;
	    if (alloc_len < sizeof(XIMStyles))
		return False;
	    if (!(p = Xmalloc(alloc_len)))
		return False;

	    rep   = (XIMStyles *)p;
	    style = (XIMStyle *)(p + sizeof(XIMStyles));

	    for (i = 0; i < num; i++)
		style[i] = (XIMStyle)style_list[i];

	    rep->count_styles = (unsigned short)num;
	    rep->supported_styles = style;
	    *((XIMStyles **)value) = rep;
	    break;
	}

    case XimType_XRectangle:
	{
	    XRectangle	*rep;

	    if (!(value))
		return False;

	    if (!(rep = Xmalloc(sizeof(XRectangle))))
		return False;

	    rep->x      = data[0];
	    rep->y      = data[1];
	    rep->width  = data[2];
	    rep->height = data[3];
	    *((XRectangle **)value) = rep;
	    break;
	}

    case XimType_XPoint:
	{
	    XPoint	*rep;

	    if (!(value))
		return False;

	    if (!(rep = Xmalloc(sizeof(XPoint))))
		return False;

	    rep->x = data[0];
	    rep->y = data[1];
	    *((XPoint **)value) = rep;
	    break;
	}

    case XimType_XFontSet:
	{
	    CARD16	 len = data[0];
	    char	*base_name;
	    XFontSet	 rep = (XFontSet)NULL;
	    char	**missing_list = NULL;
	    int		 missing_count;
	    char	*def_string;

	    if (!(value))
		return False;
	    if (!ic)
		return False;
	    if (len > data_len)
		return False;
	    if (!(base_name = Xmalloc(len + 1)))
		return False;

	    (void)strncpy(base_name, (char *)&data[1], (size_t)len);
	    base_name[len] = '\0';

	    if (mode & XIM_PREEDIT_ATTR) {
		if (!strcmp(base_name, ic->private.proto.preedit_font)) {
		    rep = ic->core.preedit_attr.fontset;
		} else if (!ic->private.proto.preedit_font_length) {
		    rep = XCreateFontSet(ic->core.im->core.display,
					base_name, &missing_list,
					&missing_count, &def_string);
		}
	    } else if (mode & XIM_STATUS_ATTR) {
		if (!strcmp(base_name, ic->private.proto.status_font)) {
		    rep = ic->core.status_attr.fontset;
		} else if (!ic->private.proto.status_font_length) {
		    rep = XCreateFontSet(ic->core.im->core.display,
					base_name, &missing_list,
					&missing_count, &def_string);
		}
	    }

	    Xfree(base_name);
	    Xfree(missing_list);
	    *((XFontSet *)value) = rep;
	    break;
	}

    case XimType_XIMHotKeyTriggers:
	{
	    CARD32			 num = *((CARD32 *)data);
	    register CARD32		*key_list = (CARD32 *)&data[2];
	    XIMHotKeyTrigger		*key;
	    XIMHotKeyTriggers		*rep;
	    register int		 i;
	    char			*p;
	    unsigned int		 alloc_len;

	    if (!(value))
		return False;

	    if (num > (UINT_MAX / sizeof(XIMHotKeyTrigger)))
		return False;
	    if ((sizeof(num) + (num * sizeof(XIMHotKeyTrigger))) > data_len)
		return False;
	    alloc_len = sizeof(XIMHotKeyTriggers)
		      + sizeof(XIMHotKeyTrigger) * num;
	    if (alloc_len < sizeof(XIMHotKeyTriggers))
		return False;
	    if (!(p = Xmalloc(alloc_len)))
		return False;

	    rep = (XIMHotKeyTriggers *)p;
	    key = (XIMHotKeyTrigger *)(p + sizeof(XIMHotKeyTriggers));

	    for (i = 0; i < num; i++, key_list += 3) {
		key[i].keysym        = (KeySym)key_list[0]; /* keysym */
		key[i].modifier      = (int)key_list[1];    /* modifier */
		key[i].modifier_mask = (int)key_list[2];    /* modifier_mask */
	    }

	    rep->num_hot_key = (int)num;
	    rep->key = key;
	    *((XIMHotKeyTriggers **)value) = rep;
	    break;
	}

    case XimType_XIMStringConversion:
	{
	    break;
	}

    default:
	return False;
    }
    return True;
}