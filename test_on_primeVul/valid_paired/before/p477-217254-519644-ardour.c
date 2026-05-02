static XMLSharedNodeList* find_impl(xmlXPathContext* ctxt, const string& xpath)
{
	xmlXPathObject* result = xmlXPathEval((const xmlChar*)xpath.c_str(), ctxt);

	if (!result) {
		xmlXPathFreeContext(ctxt);
		xmlFreeDoc(ctxt->doc);

		throw XMLException("Invalid XPath: " + xpath);
	}

	if (result->type != XPATH_NODESET) {
		xmlXPathFreeObject(result);
		xmlXPathFreeContext(ctxt);
		xmlFreeDoc(ctxt->doc);

		throw XMLException("Only nodeset result types are supported.");
	}

	xmlNodeSet* nodeset = result->nodesetval;
	XMLSharedNodeList* nodes = new XMLSharedNodeList();
	if (nodeset) {
		for (int i = 0; i < nodeset->nodeNr; ++i) {
			XMLNode* node = readnode(nodeset->nodeTab[i]);
			nodes->push_back(boost::shared_ptr<XMLNode>(node));
		}
	} else {
		// return empty set
	}

	xmlXPathFreeObject(result);

	return nodes;
}