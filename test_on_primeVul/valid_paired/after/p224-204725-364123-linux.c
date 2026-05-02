void __split_huge_pmd(struct vm_area_struct *vma, pmd_t *pmd,
		unsigned long address, bool freeze, struct page *page)
{
	spinlock_t *ptl;
	struct mmu_notifier_range range;
	bool was_locked = false;
	pmd_t _pmd;

	mmu_notifier_range_init(&range, MMU_NOTIFY_CLEAR, 0, vma, vma->vm_mm,
				address & HPAGE_PMD_MASK,
				(address & HPAGE_PMD_MASK) + HPAGE_PMD_SIZE);
	mmu_notifier_invalidate_range_start(&range);
	ptl = pmd_lock(vma->vm_mm, pmd);

	/*
	 * If caller asks to setup a migration entries, we need a page to check
	 * pmd against. Otherwise we can end up replacing wrong page.
	 */
	VM_BUG_ON(freeze && !page);
	if (page) {
		VM_WARN_ON_ONCE(!PageLocked(page));
		was_locked = true;
		if (page != pmd_page(*pmd))
			goto out;
	}

repeat:
	if (pmd_trans_huge(*pmd)) {
		if (!page) {
			page = pmd_page(*pmd);
			if (unlikely(!trylock_page(page))) {
				get_page(page);
				_pmd = *pmd;
				spin_unlock(ptl);
				lock_page(page);
				spin_lock(ptl);
				if (unlikely(!pmd_same(*pmd, _pmd))) {
					unlock_page(page);
					put_page(page);
					page = NULL;
					goto repeat;
				}
				put_page(page);
			}
		}
		if (PageMlocked(page))
			clear_page_mlock(page);
	} else if (!(pmd_devmap(*pmd) || is_pmd_migration_entry(*pmd)))
		goto out;
	__split_huge_pmd_locked(vma, pmd, range.start, freeze);
out:
	spin_unlock(ptl);
	if (!was_locked && page)
		unlock_page(page);
	/*
	 * No need to double call mmu_notifier->invalidate_range() callback.
	 * They are 3 cases to consider inside __split_huge_pmd_locked():
	 *  1) pmdp_huge_clear_flush_notify() call invalidate_range() obvious
	 *  2) __split_huge_zero_page_pmd() read only zero page and any write
	 *    fault will trigger a flush_notify before pointing to a new page
	 *    (it is fine if the secondary mmu keeps pointing to the old zero
	 *    page in the meantime)
	 *  3) Split a huge pmd into pte pointing to the same page. No need
	 *     to invalidate secondary tlb entry they are all still valid.
	 *     any further changes to individual pte will notify. So no need
	 *     to call mmu_notifier->invalidate_range()
	 */
	mmu_notifier_invalidate_range_only_end(&range);
}