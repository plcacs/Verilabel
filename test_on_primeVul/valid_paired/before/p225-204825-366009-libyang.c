lyxml_parse_elem(struct ly_ctx *ctx, const char *data, unsigned int *len, struct lyxml_elem *parent, int options)
{
    const char *c = data, *start, *e;
    const char *lws;    /* leading white space for handling mixed content */
    int uc;
    char *str;
    char *prefix = NULL;
    unsigned int prefix_len = 0;
    struct lyxml_elem *elem = NULL, *child;
    struct lyxml_attr *attr;
    unsigned int size;
    int nons_flag = 0, closed_flag = 0;

    *len = 0;

    if (*c != '<') {
        return NULL;
    }

    /* locate element name */
    c++;
    e = c;

    uc = lyxml_getutf8(ctx, e, &size);
    if (!is_xmlnamestartchar(uc)) {
        LOGVAL(ctx, LYE_XML_INVAL, LY_VLOG_NONE, NULL, "NameStartChar of the element");
        return NULL;
    }
    e += size;
    uc = lyxml_getutf8(ctx, e, &size);
    while (is_xmlnamechar(uc)) {
        if (*e == ':') {
            if (prefix_len) {
                LOGVAL(ctx, LYE_XML_INVAL, LY_VLOG_NONE, NULL, "element name, multiple colons found");
                goto error;
            }
            /* element in a namespace */
            start = e + 1;

            /* look for the prefix in namespaces */
            prefix_len = e - c;
            LY_CHECK_ERR_GOTO(prefix, LOGVAL(ctx, LYE_XML_INCHAR, LY_VLOG_NONE, NULL, e), error);
            prefix = malloc((prefix_len + 1) * sizeof *prefix);
            LY_CHECK_ERR_GOTO(!prefix, LOGMEM(ctx), error);
            memcpy(prefix, c, prefix_len);
            prefix[prefix_len] = '\0';
            c = start;
        }
        e += size;
        uc = lyxml_getutf8(ctx, e, &size);
    }
    if (!*e) {
        LOGVAL(ctx, LYE_EOF, LY_VLOG_NONE, NULL);
        free(prefix);
        return NULL;
    }

    /* allocate element structure */
    elem = calloc(1, sizeof *elem);
    LY_CHECK_ERR_RETURN(!elem, free(prefix); LOGMEM(ctx), NULL);

    elem->next = NULL;
    elem->prev = elem;
    if (parent) {
        lyxml_add_child(ctx, parent, elem);
    }

    /* store the name into the element structure */
    elem->name = lydict_insert(ctx, c, e - c);
    c = e;

process:
    ign_xmlws(c);
    if (!strncmp("/>", c, 2)) {
        /* we are done, it was EmptyElemTag */
        c += 2;
        elem->content = lydict_insert(ctx, "", 0);
        closed_flag = 1;
    } else if (*c == '>') {
        /* process element content */
        c++;
        lws = NULL;

        while (*c) {
            if (!strncmp(c, "</", 2)) {
                if (lws && !elem->child) {
                    /* leading white spaces were actually content */
                    goto store_content;
                }

                /* Etag */
                c += 2;
                /* get name and check it */
                e = c;
                uc = lyxml_getutf8(ctx, e, &size);
                if (!is_xmlnamestartchar(uc)) {
                    LOGVAL(ctx, LYE_XML_INVAL, LY_VLOG_XML, elem, "NameStartChar of the element");
                    goto error;
                }
                e += size;
                uc = lyxml_getutf8(ctx, e, &size);
                while (is_xmlnamechar(uc)) {
                    if (*e == ':') {
                        /* element in a namespace */
                        start = e + 1;

                        /* look for the prefix in namespaces */
                        if (!prefix || memcmp(prefix, c, e - c)) {
                            LOGVAL(ctx, LYE_SPEC, LY_VLOG_XML, elem,
                                   "Invalid (different namespaces) opening (%s) and closing element tags.", elem->name);
                            goto error;
                        }
                        c = start;
                    }
                    e += size;
                    uc = lyxml_getutf8(ctx, e, &size);
                }
                if (!*e) {
                    LOGVAL(ctx, LYE_EOF, LY_VLOG_NONE, NULL);
                    goto error;
                }

                /* check that it corresponds to opening tag */
                size = e - c;
                str = malloc((size + 1) * sizeof *str);
                LY_CHECK_ERR_GOTO(!str, LOGMEM(ctx), error);
                memcpy(str, c, e - c);
                str[e - c] = '\0';
                if (size != strlen(elem->name) || memcmp(str, elem->name, size)) {
                    LOGVAL(ctx, LYE_SPEC, LY_VLOG_XML, elem,
                           "Invalid (mixed names) opening (%s) and closing (%s) element tags.", elem->name, str);
                    free(str);
                    goto error;
                }
                free(str);
                c = e;

                ign_xmlws(c);
                if (*c != '>') {
                    LOGVAL(ctx, LYE_SPEC, LY_VLOG_XML, elem, "Data after closing element tag \"%s\".", elem->name);
                    goto error;
                }
                c++;
                if (!(elem->flags & LYXML_ELEM_MIXED) && !elem->content) {
                    /* there was no content, but we don't want NULL (only if mixed content) */
                    elem->content = lydict_insert(ctx, "", 0);
                }
                closed_flag = 1;
                break;

            } else if (!strncmp(c, "<?", 2)) {
                if (lws) {
                    /* leading white spaces were only formatting */
                    lws = NULL;
                }
                /* PI - ignore it */
                c += 2;
                if (parse_ignore(ctx, c, "?>", &size)) {
                    goto error;
                }
                c += size;
            } else if (!strncmp(c, "<!--", 4)) {
                if (lws) {
                    /* leading white spaces were only formatting */
                    lws = NULL;
                }
                /* Comment - ignore it */
                c += 4;
                if (parse_ignore(ctx, c, "-->", &size)) {
                    goto error;
                }
                c += size;
            } else if (!strncmp(c, "<![CDATA[", 9)) {
                /* CDSect */
                goto store_content;
            } else if (*c == '<') {
                if (lws) {
                    if (elem->flags & LYXML_ELEM_MIXED) {
                        /* we have a mixed content */
                        goto store_content;
                    } else {
                        /* leading white spaces were only formatting */
                        lws = NULL;
                    }
                }
                if (elem->content) {
                    /* we have a mixed content */
                    if (options & LYXML_PARSE_NOMIXEDCONTENT) {
                        LOGVAL(ctx, LYE_XML_INVAL, LY_VLOG_XML, elem, "XML element with mixed content");
                        goto error;
                    }
                    child = calloc(1, sizeof *child);
                    LY_CHECK_ERR_GOTO(!child, LOGMEM(ctx), error);
                    child->content = elem->content;
                    elem->content = NULL;
                    lyxml_add_child(ctx, elem, child);
                    elem->flags |= LYXML_ELEM_MIXED;
                }
                child = lyxml_parse_elem(ctx, c, &size, elem, options);
                if (!child) {
                    goto error;
                }
                c += size;      /* move after processed child element */
            } else if (is_xmlws(*c)) {
                lws = c;
                ign_xmlws(c);
            } else {
store_content:
                /* store text content */
                if (lws) {
                    /* process content including the leading white spaces */
                    c = lws;
                    lws = NULL;
                }
                str = parse_text(ctx, c, '<', &size);
                if (!str && !size) {
                    goto error;
                }
                elem->content = lydict_insert_zc(ctx, str);
                c += size;      /* move after processed text content */

                if (elem->child) {
                    /* we have a mixed content */
                    if (options & LYXML_PARSE_NOMIXEDCONTENT) {
                        LOGVAL(ctx, LYE_XML_INVAL, LY_VLOG_XML, elem, "XML element with mixed content");
                        goto error;
                    }
                    child = calloc(1, sizeof *child);
                    LY_CHECK_ERR_GOTO(!child, LOGMEM(ctx), error);
                    child->content = elem->content;
                    elem->content = NULL;
                    lyxml_add_child(ctx, elem, child);
                    elem->flags |= LYXML_ELEM_MIXED;
                }
            }
        }
    } else {
        /* process attribute */
        attr = parse_attr(ctx, c, &size, elem);
        if (!attr) {
            goto error;
        }
        c += size;              /* move after processed attribute */

        /* check namespace */
        if (attr->type == LYXML_ATTR_NS) {
            if ((!prefix || !prefix[0]) && !attr->name) {
                if (attr->value) {
                    /* default prefix */
                    elem->ns = (struct lyxml_ns *)attr;
                } else {
                    /* xmlns="" -> no namespace */
                    nons_flag = 1;
                }
            } else if (prefix && prefix[0] && attr->name && !strncmp(attr->name, prefix, prefix_len + 1)) {
                /* matching namespace with prefix */
                elem->ns = (struct lyxml_ns *)attr;
            }
        }

        /* go back to finish element processing */
        goto process;
    }

    *len = c - data;

    if (!closed_flag) {
        LOGVAL(ctx, LYE_XML_MISS, LY_VLOG_XML, elem, "closing element tag", elem->name);
        goto error;
    }

    /* resolve all attribute prefixes */
    LY_TREE_FOR(elem->attr, attr) {
        if (attr->type == LYXML_ATTR_STD_UNRES) {
            str = (char *)attr->ns;
            attr->ns = lyxml_get_ns(elem, str);
            free(str);
            attr->type = LYXML_ATTR_STD;
        }
    }

    if (!elem->ns && !nons_flag && parent) {
        elem->ns = lyxml_get_ns(parent, prefix_len ? prefix : NULL);
    }
    free(prefix);
    return elem;

error:
    lyxml_free(ctx, elem);
    free(prefix);
    return NULL;
}