static int hva_to_pfn_remapped(struct vm_area_struct *vma,
			       unsigned long addr, bool *async,
			       bool write_fault, bool *writable,
			       kvm_pfn_t *p_pfn)
{
	kvm_pfn_t pfn;
	pte_t *ptep;
	spinlock_t *ptl;
	int r;

	r = follow_pte(vma->vm_mm, addr, &ptep, &ptl);
	if (r) {
		/*
		 * get_user_pages fails for VM_IO and VM_PFNMAP vmas and does
		 * not call the fault handler, so do it here.
		 */
		bool unlocked = false;
		r = fixup_user_fault(current->mm, addr,
				     (write_fault ? FAULT_FLAG_WRITE : 0),
				     &unlocked);
		if (unlocked)
			return -EAGAIN;
		if (r)
			return r;

		r = follow_pte(vma->vm_mm, addr, &ptep, &ptl);
		if (r)
			return r;
	}

	if (write_fault && !pte_write(*ptep)) {
		pfn = KVM_PFN_ERR_RO_FAULT;
		goto out;
	}

	if (writable)
		*writable = pte_write(*ptep);
	pfn = pte_pfn(*ptep);

	/*
	 * Get a reference here because callers of *hva_to_pfn* and
	 * *gfn_to_pfn* ultimately call kvm_release_pfn_clean on the
	 * returned pfn.  This is only needed if the VMA has VM_MIXEDMAP
	 * set, but the kvm_get_pfn/kvm_release_pfn_clean pair will
	 * simply do nothing for reserved pfns.
	 *
	 * Whoever called remap_pfn_range is also going to call e.g.
	 * unmap_mapping_range before the underlying pages are freed,
	 * causing a call to our MMU notifier.
	 */ 
	kvm_get_pfn(pfn);

out:
	pte_unmap_unlock(ptep, ptl);
	*p_pfn = pfn;
	return 0;
}