static int json_internal_read_object(const char *cp,
				     const struct json_attr_t *attrs,
				     const struct json_array_t *parent,
				     int offset,
				     const char **end)
{
    enum
    { init, await_attr, in_attr, await_value, in_val_string,
	in_escape, in_val_token, post_val, post_array
    } state = 0;
#ifdef CLIENTDEBUG_ENABLE
    char *statenames[] = {
	"init", "await_attr", "in_attr", "await_value", "in_val_string",
	"in_escape", "in_val_token", "post_val", "post_array",
    };
#endif /* CLIENTDEBUG_ENABLE */
    char attrbuf[JSON_ATTR_MAX + 1], *pattr = NULL;
    char valbuf[JSON_VAL_MAX + 1], *pval = NULL;
    bool value_quoted = false;
    char uescape[5];		/* enough space for 4 hex digits and a NUL */
    const struct json_attr_t *cursor;
    int substatus, n, maxlen = 0;
    unsigned int u;
    const struct json_enum_t *mp;
    char *lptr;

    if (end != NULL)
	*end = NULL;	/* give it a well-defined value on parse failure */

    /* stuff fields with defaults in case they're omitted in the JSON input */
    for (cursor = attrs; cursor->attribute != NULL; cursor++)
	if (!cursor->nodefault) {
	    lptr = json_target_address(cursor, parent, offset);
	    if (lptr != NULL)
		switch (cursor->type) {
		case t_integer:
		    memcpy(lptr, &cursor->dflt.integer, sizeof(int));
		    break;
		case t_uinteger:
		    memcpy(lptr, &cursor->dflt.uinteger, sizeof(unsigned int));
		    break;
		case t_short:
		    memcpy(lptr, &cursor->dflt.shortint, sizeof(short));
		    break;
		case t_ushort:
		    memcpy(lptr, &cursor->dflt.ushortint,
		           sizeof(unsigned short));
		    break;
		case t_time:
		case t_real:
		    memcpy(lptr, &cursor->dflt.real, sizeof(double));
		    break;
		case t_string:
		    if (parent != NULL
			&& parent->element_type != t_structobject
			&& offset > 0)
			return JSON_ERR_NOPARSTR;
		    lptr[0] = '\0';
		    break;
		case t_boolean:
		    memcpy(lptr, &cursor->dflt.boolean, sizeof(bool));
		    break;
		case t_character:
		    lptr[0] = cursor->dflt.character;
		    break;
		case t_object:	/* silences a compiler warning */
		case t_structobject:
		case t_array:
		case t_check:
		case t_ignore:
		    break;
		}
	}

    json_debug_trace((1, "JSON parse of '%s' begins.\n", cp));

    /* parse input JSON */
    for (; *cp != '\0'; cp++) {
	json_debug_trace((2, "State %-14s, looking at '%c' (%p)\n",
			  statenames[state], *cp, cp));
	switch (state) {
	case init:
	    if (isspace((unsigned char) *cp))
		continue;
	    else if (*cp == '{')
		state = await_attr;
	    else {
		json_debug_trace((1,
				  "Non-WS when expecting object start.\n"));
#ifndef JSON_MINIMAL
		if (end != NULL)
		    *end = cp;
#endif /* JSON_MINIMAL */
		return JSON_ERR_OBSTART;
	    }
	    break;
	case await_attr:
	    if (isspace((unsigned char) *cp))
		continue;
	    else if (*cp == '"') {
		state = in_attr;
		pattr = attrbuf;
#ifndef JSON_MINIMAL
		if (end != NULL)
		    *end = cp;
#endif /* JSON_MINIMAL */
	    } else if (*cp == '}')
		break;
	    else {
		json_debug_trace((1, "Non-WS when expecting attribute.\n"));
#ifndef JSON_MINIMAL
		if (end != NULL)
		    *end = cp;
#endif /* JSON_MINIMAL */
		return JSON_ERR_ATTRSTART;
	    }
	    break;
	case in_attr:
	    if (pattr == NULL)
		/* don't update end here, leave at attribute start */
		return JSON_ERR_NULLPTR;
	    if (*cp == '"') {
		*pattr++ = '\0';
		json_debug_trace((1, "Collected attribute name %s\n",
				  attrbuf));
		for (cursor = attrs; cursor->attribute != NULL; cursor++) {
		    json_debug_trace((2, "Checking against %s\n",
				      cursor->attribute));
		    if (strcmp(cursor->attribute, attrbuf) == 0)
			break;
		}
		if (cursor->attribute == NULL) {
		    json_debug_trace((1,
				      "Unknown attribute name '%s'"
                                      " (attributes begin with '%s').\n",
				      attrbuf, attrs->attribute));
		    /* don't update end here, leave at attribute start */
		    return JSON_ERR_BADATTR;
		}
		state = await_value;
		if (cursor->type == t_string)
		    maxlen = (int)cursor->len - 1;
		else if (cursor->type == t_check)
		    maxlen = (int)strlen(cursor->dflt.check);
		else if (cursor->type == t_time || cursor->type == t_ignore)
		    maxlen = JSON_VAL_MAX;
		else if (cursor->map != NULL)
		    maxlen = (int)sizeof(valbuf) - 1;
		pval = valbuf;
	    } else if (pattr >= attrbuf + JSON_ATTR_MAX - 1) {
		json_debug_trace((1, "Attribute name too long.\n"));
		/* don't update end here, leave at attribute start */
		return JSON_ERR_ATTRLEN;
	    } else
		*pattr++ = *cp;
	    break;
	case await_value:
	    if (isspace((unsigned char) *cp) || *cp == ':')
		continue;
	    else if (*cp == '[') {
		if (cursor->type != t_array) {
		    json_debug_trace((1,
				      "Saw [ when not expecting array.\n"));
#ifndef JSON_MINIMAL
		    if (end != NULL)
			*end = cp;
#endif /* JSON_MINIMAL */
		    return JSON_ERR_NOARRAY;
		}
		substatus = json_read_array(cp, &cursor->addr.array, &cp);
		if (substatus != 0)
		    return substatus;
		state = post_array;
	    } else if (cursor->type == t_array) {
		json_debug_trace((1,
				  "Array element was specified, but no [.\n"));
#ifndef JSON_MINIMAL
		if (end != NULL)
		    *end = cp;
#endif /* JSON_MINIMAL */
		return JSON_ERR_NOBRAK;
	    } else if (*cp == '"') {
		value_quoted = true;
		state = in_val_string;
		pval = valbuf;
	    } else {
		value_quoted = false;
		state = in_val_token;
		pval = valbuf;
		*pval++ = *cp;
	    }
	    break;
	case in_val_string:
	    if (pval == NULL)
		/* don't update end here, leave at value start */
		return JSON_ERR_NULLPTR;
	    if (*cp == '\\')
		state = in_escape;
	    else if (*cp == '"') {
		*pval++ = '\0';
		json_debug_trace((1, "Collected string value %s\n", valbuf));
		state = post_val;
	    } else if (pval > valbuf + JSON_VAL_MAX - 1
		       || pval > valbuf + maxlen) {
		json_debug_trace((1, "String value too long.\n"));
		/* don't update end here, leave at value start */
		return JSON_ERR_STRLONG;	/*  */
	    } else
		*pval++ = *cp;
	    break;
	case in_escape:
	    if (pval == NULL)
		/* don't update end here, leave at value start */
		return JSON_ERR_NULLPTR;
	    switch (*cp) {
	    case 'b':
		*pval++ = '\b';
		break;
	    case 'f':
		*pval++ = '\f';
		break;
	    case 'n':
		*pval++ = '\n';
		break;
	    case 'r':
		*pval++ = '\r';
		break;
	    case 't':
		*pval++ = '\t';
		break;
	    case 'u':
                cp++;                   /* skip the 'u' */
		for (n = 0; n < 4 && isxdigit(*cp); n++)
		    uescape[n] = *cp++;
                uescape[n] = '\0';      /* terminate */
		--cp;
		if (1 != sscanf(uescape, "%4x", &u)) {
		    return JSON_ERR_BADSTRING;
                }
		*pval++ = (char)u;  /* will truncate values above 0xff */
		break;
	    default:		/* handles double quote and solidus */
		*pval++ = *cp;
		break;
	    }
	    state = in_val_string;
	    break;
	case in_val_token:
	    if (pval == NULL)
		/* don't update end here, leave at value start */
		return JSON_ERR_NULLPTR;
	    if (isspace((unsigned char) *cp) || *cp == ',' || *cp == '}') {
		*pval = '\0';
		json_debug_trace((1, "Collected token value %s.\n", valbuf));
		state = post_val;
		if (*cp == '}' || *cp == ',')
		    --cp;
	    } else if (pval > valbuf + JSON_VAL_MAX - 1) {
		json_debug_trace((1, "Token value too long.\n"));
		/* don't update end here, leave at value start */
		return JSON_ERR_TOKLONG;
	    } else
		*pval++ = *cp;
	    break;
	    /* coverity[unterminated_case] */
	case post_val:
	    /*
	     * We know that cursor points at the first spec matching
	     * the current attribute.  We don't know that it's *the*
	     * correct spec; our dialect allows there to be any number
	     * of adjacent ones with the same attrname but different
	     * types.  Here's where we try to seek forward for a
	     * matching type/attr pair if we're not looking at one.
	     */
	    for (;;) {
		int seeking = cursor->type;
		if (value_quoted && (cursor->type == t_string
                    || cursor->type == t_time))
		    break;
		if ((strcmp(valbuf, "true")==0 || strcmp(valbuf, "false")==0)
			&& seeking == t_boolean)
		    break;
		if (isdigit((unsigned char) valbuf[0])) {
		    bool decimal = strchr(valbuf, '.') != NULL;
		    if (decimal && seeking == t_real)
			break;
		    if (!decimal && (seeking == t_integer
                                     || seeking == t_uinteger))
			break;
		}
		if (cursor[1].attribute==NULL)	/* out of possiblities */
		    break;
		if (strcmp(cursor[1].attribute, attrbuf)!=0)
		    break;
		++cursor;
	    }
	    if (value_quoted
		&& (cursor->type != t_string && cursor->type != t_character
		    && cursor->type != t_check && cursor->type != t_time
		    && cursor->type != t_ignore && cursor->map == 0)) {
		json_debug_trace((1, "Saw quoted value when expecting"
                                  " non-string.\n"));
		return JSON_ERR_QNONSTRING;
	    }
	    if (!value_quoted
		&& (cursor->type == t_string || cursor->type == t_check
		    || cursor->type == t_time || cursor->map != 0)) {
		json_debug_trace((1, "Didn't see quoted value when expecting"
                                  " string.\n"));
		return JSON_ERR_NONQSTRING;
	    }
	    if (cursor->map != 0) {
		for (mp = cursor->map; mp->name != NULL; mp++)
		    if (strcmp(mp->name, valbuf) == 0) {
			goto foundit;
		    }
		json_debug_trace((1, "Invalid enumerated value string %s.\n",
				  valbuf));
		return JSON_ERR_BADENUM;
	      foundit:
		(void)snprintf(valbuf, sizeof(valbuf), "%d", mp->value);
	    }
	    lptr = json_target_address(cursor, parent, offset);
	    if (lptr != NULL)
		switch (cursor->type) {
		case t_integer:
		    {
			int tmp = atoi(valbuf);
			memcpy(lptr, &tmp, sizeof(int));
		    }
		    break;
		case t_uinteger:
		    {
			unsigned int tmp = (unsigned int)atoi(valbuf);
			memcpy(lptr, &tmp, sizeof(unsigned int));
		    }
		    break;
		case t_short:
		    {
			short tmp = atoi(valbuf);
			memcpy(lptr, &tmp, sizeof(short));
		    }
		    break;
		case t_ushort:
		    {
			unsigned short tmp = (unsigned int)atoi(valbuf);
			memcpy(lptr, &tmp, sizeof(unsigned short));
		    }
		    break;
		case t_time:
		    {
			double tmp = iso8601_to_unix(valbuf);
			memcpy(lptr, &tmp, sizeof(double));
		    }
		    break;
		case t_real:
		    {
			double tmp = safe_atof(valbuf);
			memcpy(lptr, &tmp, sizeof(double));
		    }
		    break;
		case t_string:
		    if (parent != NULL
			&& parent->element_type != t_structobject
			&& offset > 0)
			return JSON_ERR_NOPARSTR;
		    (void)strlcpy(lptr, valbuf, cursor->len);
		    break;
		case t_boolean:
		    {
			bool tmp = (strcmp(valbuf, "true") == 0);
			memcpy(lptr, &tmp, sizeof(bool));
		    }
		    break;
		case t_character:
		    if (strlen(valbuf) > 1)
			/* don't update end here, leave at value start */
			return JSON_ERR_STRLONG;
		    else
			lptr[0] = valbuf[0];
		    break;
		case t_ignore:	/* silences a compiler warning */
		case t_object:	/* silences a compiler warning */
		case t_structobject:
		case t_array:
		    break;
		case t_check:
		    if (strcmp(cursor->dflt.check, valbuf) != 0) {
			json_debug_trace((1, "Required attribute value %s"
                                          " not present.\n",
					  cursor->dflt.check));
			/* don't update end here, leave at start of attribute */
			return JSON_ERR_CHECKFAIL;
		    }
		    break;
		}
	    __attribute__ ((fallthrough));
	case post_array:
	    if (isspace((unsigned char) *cp))
		continue;
	    else if (*cp == ',')
		state = await_attr;
	    else if (*cp == '}') {
		++cp;
		goto good_parse;
	    } else {
		json_debug_trace((1, "Garbage while expecting comma or }\n"));
#ifndef JSON_MINIMAL
		if (end != NULL)
		    *end = cp;
#endif /* JSON_MINIMAL */
		return JSON_ERR_BADTRAIL;
	    }
	    break;
	}
    }

  good_parse:
    /* in case there's another object following, consume trailing WS */
    while (isspace((unsigned char) *cp))
	++cp;
    if (end != NULL)
	*end = cp;
    json_debug_trace((1, "JSON parse ends.\n"));
    return 0;
}