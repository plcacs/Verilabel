htmlParsePubidLiteral(htmlParserCtxtPtr ctxt) {
    size_t len = 0, startPosition = 0;
    xmlChar *ret = NULL;
    /*
     * Name ::= (Letter | '_') (NameChar)*
     */
    if (CUR == '"') {
        NEXT;

        if (CUR_PTR < BASE_PTR)
            return(ret);
        startPosition = CUR_PTR - BASE_PTR;

        while (IS_PUBIDCHAR_CH(CUR)) {
            len++;
            NEXT;
        }

	if (CUR != '"') {
	    htmlParseErr(ctxt, XML_ERR_LITERAL_NOT_FINISHED,
	                 "Unfinished PubidLiteral\n", NULL, NULL);
	} else {
	    ret = xmlStrndup((BASE_PTR + startPosition), len);
	    NEXT;
	}
    } else if (CUR == '\'') {
        NEXT;

        if (CUR_PTR < BASE_PTR)
            return(ret);
        startPosition = CUR_PTR - BASE_PTR;

        while ((IS_PUBIDCHAR_CH(CUR)) && (CUR != '\'')){
            len++;
            NEXT;
        }

	if (CUR != '\'') {
	    htmlParseErr(ctxt, XML_ERR_LITERAL_NOT_FINISHED,
	                 "Unfinished PubidLiteral\n", NULL, NULL);
	} else {
	    ret = xmlStrndup((BASE_PTR + startPosition), len);
	    NEXT;
	}
    } else {
	htmlParseErr(ctxt, XML_ERR_LITERAL_NOT_STARTED,
	             "PubidLiteral \" or ' expected\n", NULL, NULL);
    }

    return(ret);
}