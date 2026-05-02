spell_read_tree(
    FILE	*fd,
    char_u	**bytsp,
    idx_T	**idxsp,
    int		prefixtree,	/* TRUE for the prefix tree */
    int		prefixcnt)	/* when "prefixtree" is TRUE: prefix count */
{
    int		len;
    int		idx;
    char_u	*bp;
    idx_T	*ip;

    /* The tree size was computed when writing the file, so that we can
     * allocate it as one long block. <nodecount> */
    len = get4c(fd);
    if (len < 0)
	return SP_TRUNCERROR;
    if (len > 0)
    {
	/* Allocate the byte array. */
	bp = lalloc((long_u)len, TRUE);
	if (bp == NULL)
	    return SP_OTHERERROR;
	*bytsp = bp;

	/* Allocate the index array. */
	ip = (idx_T *)lalloc_clear((long_u)(len * sizeof(int)), TRUE);
	if (ip == NULL)
	    return SP_OTHERERROR;
	*idxsp = ip;

	/* Recursively read the tree and store it in the array. */
	idx = read_tree_node(fd, bp, ip, len, 0, prefixtree, prefixcnt);
	if (idx < 0)
	    return idx;
    }
    return 0;
}