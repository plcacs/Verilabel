sug_filltree(spellinfo_T *spin, slang_T *slang)
{
    char_u	*byts;
    idx_T	*idxs;
    int		depth;
    idx_T	arridx[MAXWLEN];
    int		curi[MAXWLEN];
    char_u	tword[MAXWLEN];
    char_u	tsalword[MAXWLEN];
    int		c;
    idx_T	n;
    unsigned	words_done = 0;
    int		wordcount[MAXWLEN];

    // We use si_foldroot for the soundfolded trie.
    spin->si_foldroot = wordtree_alloc(spin);
    if (spin->si_foldroot == NULL)
	return FAIL;

    // let tree_add_word() know we're adding to the soundfolded tree
    spin->si_sugtree = TRUE;

    /*
     * Go through the whole case-folded tree, soundfold each word and put it
     * in the trie.
     */
    byts = slang->sl_fbyts;
    idxs = slang->sl_fidxs;

    arridx[0] = 0;
    curi[0] = 1;
    wordcount[0] = 0;

    depth = 0;
    while (depth >= 0 && !got_int)
    {
	if (curi[depth] > byts[arridx[depth]])
	{
	    // Done all bytes at this node, go up one level.
	    idxs[arridx[depth]] = wordcount[depth];
	    if (depth > 0)
		wordcount[depth - 1] += wordcount[depth];

	    --depth;
	    line_breakcheck();
	}
	else
	{

	    // Do one more byte at this node.
	    n = arridx[depth] + curi[depth];
	    ++curi[depth];

	    c = byts[n];
	    if (c == 0)
	    {
		// Sound-fold the word.
		tword[depth] = NUL;
		spell_soundfold(slang, tword, TRUE, tsalword);

		// We use the "flags" field for the MSB of the wordnr,
		// "region" for the LSB of the wordnr.
		if (tree_add_word(spin, tsalword, spin->si_foldroot,
				words_done >> 16, words_done & 0xffff,
							   0) == FAIL)
		    return FAIL;

		++words_done;
		++wordcount[depth];

		// Reset the block count each time to avoid compression
		// kicking in.
		spin->si_blocks_cnt = 0;

		// Skip over any other NUL bytes (same word with different
		// flags).  But don't go over the end.
		while (n + 1 < slang->sl_fbyts_len && byts[n + 1] == 0)
		{
		    ++n;
		    ++curi[depth];
		}
	    }
	    else
	    {
		// Normal char, go one level deeper.
		tword[depth++] = c;
		arridx[depth] = idxs[n];
		curi[depth] = 1;
		wordcount[depth] = 0;
	    }
	}
    }

    smsg(_("Total number of words: %d"), words_done);

    return OK;
}