Agraph_t *agroot(void* obj)
{
    switch (AGTYPE(obj)) {
    case AGINEDGE:
    case AGOUTEDGE:
	return ((Agedge_t *) obj)->node->root;
    case AGNODE:
	return ((Agnode_t *) obj)->root;
    case AGRAPH:
	return ((Agraph_t *) obj)->root;
    default:			/* actually can't occur if only 2 bit tags */
	agerr(AGERR, "agroot of a bad object");
	return NILgraph;
    }
}