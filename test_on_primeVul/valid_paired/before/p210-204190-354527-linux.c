static inline void kvm_memslot_delete(struct kvm_memslots *slots,
				      struct kvm_memory_slot *memslot)
{
	struct kvm_memory_slot *mslots = slots->memslots;
	int i;

	if (WARN_ON(slots->id_to_index[memslot->id] == -1))
		return;

	slots->used_slots--;

	for (i = slots->id_to_index[memslot->id]; i < slots->used_slots; i++) {
		mslots[i] = mslots[i + 1];
		slots->id_to_index[mslots[i].id] = i;
	}
	mslots[i] = *memslot;
	slots->id_to_index[memslot->id] = -1;
}