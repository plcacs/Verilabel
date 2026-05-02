static pj_xml_node *xml_parse_node( pj_pool_t *pool, pj_scanner *scanner)
{
    pj_xml_node *node;
    pj_str_t end_name;

    PJ_CHECK_STACK();

    if (*scanner->curptr != '<')
	on_syntax_error(scanner);

    /* Handle Processing Instructino (PI) construct (i.e. "<?") */
    if (*scanner->curptr == '<' && *(scanner->curptr+1) == '?') {
	pj_scan_advance_n(scanner, 2, PJ_FALSE);
	for (;;) {
	    pj_str_t dummy;
	    pj_scan_get_until_ch(scanner, '?', &dummy);
	    if (*scanner->curptr=='?' && *(scanner->curptr+1)=='>') {
		pj_scan_advance_n(scanner, 2, PJ_TRUE);
		break;
	    } else {
		pj_scan_advance_n(scanner, 1, PJ_FALSE);
	    }
	}
	return xml_parse_node(pool, scanner);
    }

    /* Handle comments construct (i.e. "<!") */
    if (pj_scan_strcmp(scanner, "<!", 2) == 0) {
	pj_scan_advance_n(scanner, 2, PJ_FALSE);
	for (;;) {
	    pj_str_t dummy;
	    pj_scan_get_until_ch(scanner, '>', &dummy);
	    if (pj_scan_strcmp(scanner, ">", 1) == 0) {
		pj_scan_advance_n(scanner, 1, PJ_TRUE);
		break;
	    } else {
		pj_scan_advance_n(scanner, 1, PJ_FALSE);
	    }
	}
	return xml_parse_node(pool, scanner);
    }

    /* Alloc node. */
    node = alloc_node(pool);

    /* Get '<' */
    pj_scan_get_char(scanner);

    /* Get node name. */
    pj_scan_get_until_chr( scanner, " />\t\r\n", &node->name);

    /* Get attributes. */
    while (*scanner->curptr != '>' && *scanner->curptr != '/') {
	pj_xml_attr *attr = alloc_attr(pool);
	
	pj_scan_get_until_chr( scanner, "=> \t\r\n", &attr->name);
	if (*scanner->curptr == '=') {
	    pj_scan_get_char( scanner );
            pj_scan_get_quotes(scanner, "\"'", "\"'", 2, &attr->value);
	    /* remove quote characters */
	    ++attr->value.ptr;
	    attr->value.slen -= 2;
	}
	
	pj_list_push_back( &node->attr_head, attr );
    }

    if (*scanner->curptr == '/') {
	pj_scan_get_char(scanner);
	if (pj_scan_get_char(scanner) != '>')
	    on_syntax_error(scanner);
	return node;
    }

    /* Enclosing bracket. */
    if (pj_scan_get_char(scanner) != '>')
	on_syntax_error(scanner);

    /* Sub nodes. */
    while (*scanner->curptr == '<' && *(scanner->curptr+1) != '/'
				   && *(scanner->curptr+1) != '!')
    {
	pj_xml_node *sub_node = xml_parse_node(pool, scanner);
	pj_list_push_back( &node->node_head, sub_node );
    }

    /* Content. */
    if (!pj_scan_is_eof(scanner) && *scanner->curptr != '<') {
	pj_scan_get_until_ch(scanner, '<', &node->content);
    }

    /* CDATA content. */
    if (*scanner->curptr == '<' && *(scanner->curptr+1) == '!' &&
	pj_scan_strcmp(scanner, "<![CDATA[", 9) == 0)
    {
	pj_scan_advance_n(scanner, 9, PJ_FALSE);
	pj_scan_get_until_ch(scanner, ']', &node->content);
	while (pj_scan_strcmp(scanner, "]]>", 3)) {
	    pj_str_t dummy;
	    pj_scan_get_until_ch(scanner, ']', &dummy);
	}
	node->content.slen = scanner->curptr - node->content.ptr;
	pj_scan_advance_n(scanner, 3, PJ_TRUE);
    }

    /* Enclosing node. */
    if (pj_scan_get_char(scanner) != '<' || pj_scan_get_char(scanner) != '/')
	on_syntax_error(scanner);

    pj_scan_get_until_chr(scanner, " \t>", &end_name);

    /* Compare name. */
    if (pj_stricmp(&node->name, &end_name) != 0)
	on_syntax_error(scanner);

    /* Enclosing '>' */
    if (pj_scan_get_char(scanner) != '>')
	on_syntax_error(scanner);

    return node;
}