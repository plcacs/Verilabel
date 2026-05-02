exsltSaxonLineNumberFunction(xmlXPathParserContextPtr ctxt, int nargs) {
    xmlNodePtr cur = NULL;
    xmlXPathObjectPtr obj = NULL;
    long lineNo = -1;

    if (nargs == 0) {
	cur = ctxt->context->node;
    } else if (nargs == 1) {
	xmlNodeSetPtr nodelist;
	int i;

	if ((ctxt->value == NULL) || (ctxt->value->type != XPATH_NODESET)) {
	    xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		"saxon:line-number() : invalid arg expecting a node-set\n");
	    ctxt->error = XPATH_INVALID_TYPE;
	    return;
	}

	obj = valuePop(ctxt);
	nodelist = obj->nodesetval;
	if ((nodelist != NULL) && (nodelist->nodeNr > 0)) {
            cur = nodelist->nodeTab[0];
            for (i = 1;i < nodelist->nodeNr;i++) {
                int ret = xmlXPathCmpNodes(cur, nodelist->nodeTab[i]);
                if (ret == -1)
                    cur = nodelist->nodeTab[i];
            }
        }
    } else {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		"saxon:line-number() : invalid number of args %d\n",
		nargs);
	ctxt->error = XPATH_INVALID_ARITY;
	return;
    }

    if ((cur != NULL) && (cur->type == XML_NAMESPACE_DECL)) {
        /*
        * The XPath module sets the owner element of a ns-node on
        * the ns->next field.
        */
        cur = (xmlNodePtr) ((xmlNsPtr) cur)->next;
        if (cur == NULL || cur->type != XML_ELEMENT_NODE) {
            xsltGenericError(xsltGenericErrorContext,
                "Internal error in exsltSaxonLineNumberFunction: "
                "Cannot retrieve the doc of a namespace node.\n");
            cur = NULL;
        }
    }

    if (cur != NULL)
        lineNo = xmlGetLineNo(cur);

    valuePush(ctxt, xmlXPathNewFloat(lineNo));

    xmlXPathFreeObject(obj);
}