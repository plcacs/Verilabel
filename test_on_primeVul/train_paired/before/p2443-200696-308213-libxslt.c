exsltSaxonLineNumberFunction(xmlXPathParserContextPtr ctxt, int nargs) {
    xmlNodePtr cur = NULL;

    if (nargs == 0) {
	cur = ctxt->context->node;
    } else if (nargs == 1) {
	xmlXPathObjectPtr obj;
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
	if ((nodelist == NULL) || (nodelist->nodeNr <= 0)) {
	    xmlXPathFreeObject(obj);
	    valuePush(ctxt, xmlXPathNewFloat(-1));
	    return;
	}
	cur = nodelist->nodeTab[0];
	for (i = 1;i < nodelist->nodeNr;i++) {
	    int ret = xmlXPathCmpNodes(cur, nodelist->nodeTab[i]);
	    if (ret == -1)
		cur = nodelist->nodeTab[i];
	}
	xmlXPathFreeObject(obj);
    } else {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		"saxon:line-number() : invalid number of args %d\n",
		nargs);
	ctxt->error = XPATH_INVALID_ARITY;
	return;
    }

    valuePush(ctxt, xmlXPathNewFloat(xmlGetLineNo(cur)));
    return;
}