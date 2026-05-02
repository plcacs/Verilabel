int cli_hm_scan(const unsigned char *digest, uint32_t size, const char **virname, const struct cli_matcher *root, enum CLI_HASH_TYPE type) {
    const struct cli_htu32_element *item;
    unsigned int keylen;
    struct cli_sz_hash *szh;
    size_t l, r;

    if(!digest || !size || size == 0xffffffff || !root || !root->hm.sizehashes[type].capacity)
	return CL_CLEAN;

    item = cli_htu32_find(&root->hm.sizehashes[type], size);
    if(!item)
	return CL_CLEAN;

    szh = (struct cli_sz_hash *)item->data.as_ptr;
    keylen = hashlen[type];

    l = 0;
    r = szh->items;
    while(l <= r) {
	size_t c = (l + r) / 2;
	int res = hm_cmp(digest, &szh->hash_array[keylen * c], keylen);

	if(res < 0) {
	    if(!c)
		break;
	    r = c - 1;
	} else if(res > 0)
	    l = c + 1;
	else {
	    if(virname)
		*virname = szh->virusnames[c];
	    return CL_VIRUS;
	}
    }
    return CL_CLEAN;
}