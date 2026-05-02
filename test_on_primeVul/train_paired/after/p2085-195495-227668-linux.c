static inline void get_page(struct page *page)
{
	page = compound_head(page);
	/*
	 * Getting a normal page or the head of a compound page
	 * requires to already have an elevated page->_refcount.
	 */
	VM_BUG_ON_PAGE(page_ref_zero_or_close_to_overflow(page), page);
	page_ref_inc(page);
}