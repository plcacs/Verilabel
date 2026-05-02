static inline int pmd_none_or_trans_huge_or_clear_bad(pmd_t *pmd)
{
	/* depend on compiler for an atomic pmd read */
	pmd_t pmdval = *pmd;
	/*
	 * The barrier will stabilize the pmdval in a register or on
	 * the stack so that it will stop changing under the code.
	 */
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	barrier();
#endif
	if (pmd_none(pmdval))
		return 1;
	if (unlikely(pmd_bad(pmdval))) {
		if (!pmd_trans_huge(pmdval))
			pmd_clear_bad(pmd);
		return 1;
	}
	return 0;
}