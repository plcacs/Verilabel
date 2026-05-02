static int kvm_s390_get_cmma(struct kvm *kvm, struct kvm_s390_cmma_log *args,
			     u8 *res, unsigned long bufsize)
{
	unsigned long mem_end, cur_gfn, next_gfn, hva, pgstev;
	struct kvm_memslots *slots = kvm_memslots(kvm);
	struct kvm_memory_slot *ms;

	cur_gfn = kvm_s390_next_dirty_cmma(slots, args->start_gfn);
	ms = gfn_to_memslot(kvm, cur_gfn);
	args->count = 0;
	args->start_gfn = cur_gfn;
	if (!ms)
		return 0;
	next_gfn = kvm_s390_next_dirty_cmma(slots, cur_gfn + 1);
	mem_end = slots->memslots[0].base_gfn + slots->memslots[0].npages;

	while (args->count < bufsize) {
		hva = gfn_to_hva(kvm, cur_gfn);
		if (kvm_is_error_hva(hva))
			return 0;
		/* Decrement only if we actually flipped the bit to 0 */
		if (test_and_clear_bit(cur_gfn - ms->base_gfn, kvm_second_dirty_bitmap(ms)))
			atomic64_dec(&kvm->arch.cmma_dirty_pages);
		if (get_pgste(kvm->mm, hva, &pgstev) < 0)
			pgstev = 0;
		/* Save the value */
		res[args->count++] = (pgstev >> 24) & 0x43;
		/* If the next bit is too far away, stop. */
		if (next_gfn > cur_gfn + KVM_S390_MAX_BIT_DISTANCE)
			return 0;
		/* If we reached the previous "next", find the next one */
		if (cur_gfn == next_gfn)
			next_gfn = kvm_s390_next_dirty_cmma(slots, cur_gfn + 1);
		/* Reached the end of memory or of the buffer, stop */
		if ((next_gfn >= mem_end) ||
		    (next_gfn - args->start_gfn >= bufsize))
			return 0;
		cur_gfn++;
		/* Reached the end of the current memslot, take the next one. */
		if (cur_gfn - ms->base_gfn >= ms->npages) {
			ms = gfn_to_memslot(kvm, cur_gfn);
			if (!ms)
				return 0;
		}
	}
	return 0;
}