Mangle(input, control)		/* returns a pointer to a controlled Mangle */
    char *input;
    char *control;
{
    int limit;
    register char *ptr;
    static char area[STRINGSIZE];
    char area2[STRINGSIZE];
    area[0] = '\0';
    strcpy(area, input);

    for (ptr = control; *ptr; ptr++)
    {
	switch (*ptr)
	{
	case RULE_NOOP:
	    break;
	case RULE_REVERSE:
	    strcpy(area, Reverse(area));
	    break;
	case RULE_UPPERCASE:
	    strcpy(area, Uppercase(area));
	    break;
	case RULE_LOWERCASE:
	    strcpy(area, Lowercase(area));
	    break;
	case RULE_CAPITALISE:
	    strcpy(area, Capitalise(area));
	    break;
	case RULE_PLURALISE:
	    strcpy(area, Pluralise(area));
	    break;
	case RULE_REFLECT:
	    strcat(area, Reverse(area));
	    break;
	case RULE_DUPLICATE:
	    strcpy(area2, area);
	    strcat(area, area2);
	    break;
	case RULE_GT:
	    if (!ptr[1])
	    {
		Debug(1, "Mangle: '>' missing argument in '%s'\n", control);
		return NULL;
	    } else
	    {
		limit = Char2Int(*(++ptr));
		if (limit < 0)
		{
		    Debug(1, "Mangle: '>' weird argument in '%s'\n", control);
		    return NULL;
		}
		if ( (int) strlen(area) <= limit)
		{
		    return NULL;
		}
	    }
	    break;
	case RULE_LT:
	    if (!ptr[1])
	    {
		Debug(1, "Mangle: '<' missing argument in '%s'\n", control);
		return NULL;
	    } else
	    {
		limit = Char2Int(*(++ptr));
		if (limit < 0)
		{
		    Debug(1, "Mangle: '<' weird argument in '%s'\n", control);
		    return NULL;
		}
		if ( (int) strlen(area) >= limit)
		{
		    return NULL;
		}
	    }
	    break;
	case RULE_PREPEND:
	    if (!ptr[1])
	    {
		Debug(1, "Mangle: prepend missing argument in '%s'\n", control);
		return NULL;
	    } else
	    {
		area2[0] = *(++ptr);
		strcpy(area2 + 1, area);
		strcpy(area, area2);
	    }
	    break;
	case RULE_APPEND:
	    if (!ptr[1])
	    {
		Debug(1, "Mangle: append missing argument in '%s'\n", control);
		return NULL;
	    } else
	    {
		register char *string;
		string = area;
		while (*(string++));
		string[-1] = *(++ptr);
		*string = '\0';
	    }
	    break;
	case RULE_EXTRACT:
	    if (!ptr[1] || !ptr[2])
	    {
		Debug(1, "Mangle: extract missing argument in '%s'\n", control);
		return NULL;
	    } else
	    {
		register int i;
		int start;
		int length;
		start = Char2Int(*(++ptr));
		length = Char2Int(*(++ptr));
		if (start < 0 || length < 0)
		{
		    Debug(1, "Mangle: extract: weird argument in '%s'\n", control);
		    return NULL;
		}
		strcpy(area2, area);
		for (i = 0; length-- && area2[start + i]; i++)
		{
		    area[i] = area2[start + i];
		}
		/* cant use strncpy() - no trailing NUL */
		area[i] = '\0';
	    }
	    break;
	case RULE_OVERSTRIKE:
	    if (!ptr[1] || !ptr[2])
	    {
		Debug(1, "Mangle: overstrike missing argument in '%s'\n", control);
		return NULL;
	    } else
	    {
		register int i;
		i = Char2Int(*(++ptr));
		if (i < 0)
		{
		    Debug(1, "Mangle: overstrike weird argument in '%s'\n",
			  control);
		    return NULL;
		} else
		{
		    ++ptr;
		    if (area[i])
		    {
			area[i] = *ptr;
		    }
		}
	    }
	    break;
	case RULE_INSERT:
	    if (!ptr[1] || !ptr[2])
	    {
		Debug(1, "Mangle: insert missing argument in '%s'\n", control);
		return NULL;
	    } else
	    {
		register int i;
		register char *p1;
		register char *p2;
		i = Char2Int(*(++ptr));
		if (i < 0)
		{
		    Debug(1, "Mangle: insert weird argument in '%s'\n",
			  control);
		    return NULL;
		}
		p1 = area;
		p2 = area2;
		while (i && *p1)
		{
		    i--;
		    *(p2++) = *(p1++);
		}
		*(p2++) = *(++ptr);
		strcpy(p2, p1);
		strcpy(area, area2);
	    }
	    break;
	    /* THE FOLLOWING RULES REQUIRE CLASS MATCHING */

	case RULE_PURGE:	/* @x or @?c */
	    if (!ptr[1] || (ptr[1] == RULE_CLASS && !ptr[2]))
	    {
		Debug(1, "Mangle: delete missing arguments in '%s'\n", control);
		return NULL;
	    } else if (ptr[1] != RULE_CLASS)
	    {
		strcpy(area, Purge(area, *(++ptr)));
	    } else
	    {
		strcpy(area, PolyPurge(area, ptr[2]));
		ptr += 2;
	    }
	    break;
	case RULE_SUBSTITUTE:	/* sxy || s?cy */
	    if (!ptr[1] || !ptr[2] || (ptr[1] == RULE_CLASS && !ptr[3]))
	    {
		Debug(1, "Mangle: subst missing argument in '%s'\n", control);
		return NULL;
	    } else if (ptr[1] != RULE_CLASS)
	    {
		strcpy(area, Substitute(area, ptr[1], ptr[2]));
		ptr += 2;
	    } else
	    {
		strcpy(area, PolySubst(area, ptr[2], ptr[3]));
		ptr += 3;
	    }
	    break;
	case RULE_MATCH:	/* /x || /?c */
	    if (!ptr[1] || (ptr[1] == RULE_CLASS && !ptr[2]))
	    {
		Debug(1, "Mangle: '/' missing argument in '%s'\n", control);
		return NULL;
	    } else if (ptr[1] != RULE_CLASS)
	    {
		if (!strchr(area, *(++ptr)))
		{
		    return NULL;
		}
	    } else
	    {
		if (!PolyStrchr(area, ptr[2]))
		{
		    return NULL;
		}
		ptr += 2;
	    }
	    break;
	case RULE_NOT:		/* !x || !?c */
	    if (!ptr[1] || (ptr[1] == RULE_CLASS && !ptr[2]))
	    {
		Debug(1, "Mangle: '!' missing argument in '%s'\n", control);
		return NULL;
	    } else if (ptr[1] != RULE_CLASS)
	    {
		if (strchr(area, *(++ptr)))
		{
		    return NULL;
		}
	    } else
	    {
		if (PolyStrchr(area, ptr[2]))
		{
		    return NULL;
		}
		ptr += 2;
	    }
	    break;
	    /*
	     * alternative use for a boomerang, number 1: a standard throwing
	     * boomerang is an ideal thing to use to tuck the sheets under
	     * the mattress when making your bed.  The streamlined shape of
	     * the boomerang allows it to slip easily 'twixt mattress and
	     * bedframe, and it's curve makes it very easy to hook sheets
	     * into the gap.
	     */

	case RULE_EQUALS:	/* =nx || =n?c */
	    if (!ptr[1] || !ptr[2] || (ptr[2] == RULE_CLASS && !ptr[3]))
	    {
		Debug(1, "Mangle: '=' missing argument in '%s'\n", control);
		return NULL;
	    } else
	    {
		register int i;
		if ((i = Char2Int(ptr[1])) < 0)
		{
		    Debug(1, "Mangle: '=' weird argument in '%s'\n", control);
		    return NULL;
		}
		if (ptr[2] != RULE_CLASS)
		{
		    ptr += 2;
		    if (area[i] != *ptr)
		    {
			return NULL;
		    }
		} else
		{
		    ptr += 3;
		    if (!MatchClass(*ptr, area[i]))
		    {
			return NULL;
		    }
		}
	    }
	    break;

	case RULE_DFIRST:
	    if (area[0])
	    {
		register int i;
		for (i = 1; area[i]; i++)
		{
		    area[i - 1] = area[i];
		}
		area[i - 1] = '\0';
	    }
	    break;

	case RULE_DLAST:
	    if (area[0])
	    {
		register int i;
		for (i = 1; area[i]; i++);
		area[i - 1] = '\0';
	    }
	    break;

	case RULE_MFIRST:
	    if (!ptr[1] || (ptr[1] == RULE_CLASS && !ptr[2]))
	    {
		Debug(1, "Mangle: '(' missing argument in '%s'\n", control);
		return NULL;
	    } else
	    {
		if (ptr[1] != RULE_CLASS)
		{
		    ptr++;
		    if (area[0] != *ptr)
		    {
			return NULL;
		    }
		} else
		{
		    ptr += 2;
		    if (!MatchClass(*ptr, area[0]))
		    {
			return NULL;
		    }
		}
	    }
	case RULE_MLAST:
	    if (!ptr[1] || (ptr[1] == RULE_CLASS && !ptr[2]))
	    {
		Debug(1, "Mangle: ')' missing argument in '%s'\n", control);
		return NULL;
	    } else
	    {
		register int i;

		for (i = 0; area[i]; i++);

		if (i > 0)
		{
		    i--;
		} else
		{
		    return NULL;
		}

		if (ptr[1] != RULE_CLASS)
		{
		    ptr++;
		    if (area[i] != *ptr)
		    {
			return NULL;
		    }
		} else
		{
		    ptr += 2;
		    if (!MatchClass(*ptr, area[i]))
		    {
			return NULL;
		    }
		}
	    }

	default:
	    Debug(1, "Mangle: unknown command %c in %s\n", *ptr, control);
	    return NULL;
	    break;
	}
    }
    if (!area[0])		/* have we deweted de poor widdle fing away? */
    {
	return NULL;
    }
    return (area);
}