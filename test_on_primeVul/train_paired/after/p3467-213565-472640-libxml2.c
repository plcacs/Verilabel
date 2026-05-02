xmlXIncludeDoProcess(xmlXIncludeCtxtPtr ctxt, xmlDocPtr doc, xmlNodePtr tree,
                     int skipRoot) {
    xmlNodePtr cur;
    int ret = 0;
    int i, start;

    if ((doc == NULL) || (tree == NULL) || (tree->type == XML_NAMESPACE_DECL))
	return(-1);
    if ((skipRoot) && (tree->children == NULL))
        return(-1);
    if (ctxt == NULL)
	return(-1);

    if (doc->URL != NULL) {
	ret = xmlXIncludeURLPush(ctxt, doc->URL);
	if (ret < 0)
	    return(-1);
    }
    start = ctxt->incNr;

    /*
     * TODO: The phases must run separately for recursive inclusions.
     *
     * - Phase 1 should start with top-level XInclude nodes, load documents,
     *   execute XPointer expressions, then process only the result nodes
     *   (not whole document, see bug #324081) and only for phase 1
     *   recursively. We will need a backreference from xmlNodes to
     *   xmlIncludeRefs to detect references that were already visited.
     *   This can also be used for proper cycle detection, see bug #344240.
     *
     * - Phase 2 should visit all top-level XInclude nodes and expand
     *   possible subreferences in the replacement recursively.
     *
     * - Phase 3 should finally replace the top-level XInclude nodes.
     *   It could also be run together with phase 2.
     */

    /*
     * First phase: lookup the elements in the document
     */
    if (skipRoot)
        cur = tree->children;
    else
        cur = tree;
    do {
	/* TODO: need to work on entities -> stack */
        if (xmlXIncludeTestNode(ctxt, cur) == 1) {
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
            /*
             * Avoid superlinear expansion by limiting the total number
             * of replacements.
             */
            if (ctxt->incTotal >= 20)
                return(-1);
#endif
            ctxt->incTotal++;
            xmlXIncludePreProcessNode(ctxt, cur);
        } else if ((cur->children != NULL) &&
                   ((cur->type == XML_DOCUMENT_NODE) ||
                    (cur->type == XML_ELEMENT_NODE))) {
            cur = cur->children;
            continue;
        }
        do {
            if (cur == tree)
                break;
            if (cur->next != NULL) {
                cur = cur->next;
                break;
            }
            cur = cur->parent;
        } while (cur != NULL);
    } while ((cur != NULL) && (cur != tree));

    /*
     * Second Phase : collect the infosets fragments
     */
    for (i = start;i < ctxt->incNr; i++) {
        xmlXIncludeLoadNode(ctxt, i);
	ret++;
    }

    /*
     * Third phase: extend the original document infoset.
     *
     * Originally we bypassed the inclusion if there were any errors
     * encountered on any of the XIncludes.  A bug was raised (bug
     * 132588) requesting that we output the XIncludes without error,
     * so the check for inc!=NULL || xptr!=NULL was put in.  This may
     * give some other problems in the future, but for now it seems to
     * work ok.
     *
     */
    for (i = ctxt->incBase;i < ctxt->incNr; i++) {
	if ((ctxt->incTab[i]->inc != NULL) ||
	    (ctxt->incTab[i]->emptyFb != 0))	/* (empty fallback) */
	    xmlXIncludeIncludeNode(ctxt, i);
    }

    if (doc->URL != NULL)
	xmlXIncludeURLPop(ctxt);
    return(ret);
}