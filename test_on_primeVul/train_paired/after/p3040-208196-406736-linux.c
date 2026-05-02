static inline bool can_follow_write_pte(pte_t pte, unsigned int flags)
{
	return pte_write(pte) || ((flags & FOLL_COW) && pte_dirty(pte));
}