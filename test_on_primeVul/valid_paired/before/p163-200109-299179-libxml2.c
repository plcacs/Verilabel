xmlParseBalancedChunkMemoryRecover(xmlDocPtr doc, xmlSAXHandlerPtr sax,
     void *user_data, int depth, const xmlChar *string, xmlNodePtr *lst,
     int recover) {
    xmlParserCtxtPtr ctxt;
    xmlDocPtr newDoc;
    xmlSAXHandlerPtr oldsax = NULL;
    xmlNodePtr content, newRoot;
    int size;
    int ret = 0;

    if (depth > 40) {
	return(XML_ERR_ENTITY_LOOP);
    }


    if (lst != NULL)
        *lst = NULL;
    if (string == NULL)
        return(-1);

    size = xmlStrlen(string);

    ctxt = xmlCreateMemoryParserCtxt((char *) string, size);
    if (ctxt == NULL) return(-1);
    ctxt->userData = ctxt;
    if (sax != NULL) {
	oldsax = ctxt->sax;
        ctxt->sax = sax;
	if (user_data != NULL)
	    ctxt->userData = user_data;
    }
    newDoc = xmlNewDoc(BAD_CAST "1.0");
    if (newDoc == NULL) {
	xmlFreeParserCtxt(ctxt);
	return(-1);
    }
    newDoc->properties = XML_DOC_INTERNAL;
    if ((doc != NULL) && (doc->dict != NULL)) {
        xmlDictFree(ctxt->dict);
	ctxt->dict = doc->dict;
	xmlDictReference(ctxt->dict);
	ctxt->str_xml = xmlDictLookup(ctxt->dict, BAD_CAST "xml", 3);
	ctxt->str_xmlns = xmlDictLookup(ctxt->dict, BAD_CAST "xmlns", 5);
	ctxt->str_xml_ns = xmlDictLookup(ctxt->dict, XML_XML_NAMESPACE, 36);
	ctxt->dictNames = 1;
    } else {
	xmlCtxtUseOptionsInternal(ctxt, XML_PARSE_NODICT, NULL);
    }
    if (doc != NULL) {
	newDoc->intSubset = doc->intSubset;
	newDoc->extSubset = doc->extSubset;
    }
    newRoot = xmlNewDocNode(newDoc, NULL, BAD_CAST "pseudoroot", NULL);
    if (newRoot == NULL) {
	if (sax != NULL)
	    ctxt->sax = oldsax;
	xmlFreeParserCtxt(ctxt);
	newDoc->intSubset = NULL;
	newDoc->extSubset = NULL;
        xmlFreeDoc(newDoc);
	return(-1);
    }
    xmlAddChild((xmlNodePtr) newDoc, newRoot);
    nodePush(ctxt, newRoot);
    if (doc == NULL) {
	ctxt->myDoc = newDoc;
    } else {
	ctxt->myDoc = newDoc;
	newDoc->children->doc = doc;
	/* Ensure that doc has XML spec namespace */
	xmlSearchNsByHref(doc, (xmlNodePtr)doc, XML_XML_NAMESPACE);
	newDoc->oldNs = doc->oldNs;
    }
    ctxt->instate = XML_PARSER_CONTENT;
    ctxt->input_id = 2;
    ctxt->depth = depth;

    /*
     * Doing validity checking on chunk doesn't make sense
     */
    ctxt->validate = 0;
    ctxt->loadsubset = 0;
    xmlDetectSAX2(ctxt);

    if ( doc != NULL ){
        content = doc->children;
        doc->children = NULL;
        xmlParseContent(ctxt);
        doc->children = content;
    }
    else {
        xmlParseContent(ctxt);
    }
    if ((RAW == '<') && (NXT(1) == '/')) {
	xmlFatalErr(ctxt, XML_ERR_NOT_WELL_BALANCED, NULL);
    } else if (RAW != 0) {
	xmlFatalErr(ctxt, XML_ERR_EXTRA_CONTENT, NULL);
    }
    if (ctxt->node != newDoc->children) {
	xmlFatalErr(ctxt, XML_ERR_NOT_WELL_BALANCED, NULL);
    }

    if (!ctxt->wellFormed) {
        if (ctxt->errNo == 0)
	    ret = 1;
	else
	    ret = ctxt->errNo;
    } else {
      ret = 0;
    }

    if ((lst != NULL) && ((ret == 0) || (recover == 1))) {
	xmlNodePtr cur;

	/*
	 * Return the newly created nodeset after unlinking it from
	 * they pseudo parent.
	 */
	cur = newDoc->children->children;
	*lst = cur;
	while (cur != NULL) {
	    xmlSetTreeDoc(cur, doc);
	    cur->parent = NULL;
	    cur = cur->next;
	}
	newDoc->children->children = NULL;
    }

    if (sax != NULL)
	ctxt->sax = oldsax;
    xmlFreeParserCtxt(ctxt);
    newDoc->intSubset = NULL;
    newDoc->extSubset = NULL;
    newDoc->oldNs = NULL;
    xmlFreeDoc(newDoc);

    return(ret);
}