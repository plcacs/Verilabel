static inline void tss_invalidate_io_bitmap(struct tss_struct *tss)
{
	/*
	 * Invalidate the I/O bitmap by moving io_bitmap_base outside the
	 * TSS limit so any subsequent I/O access from user space will
	 * trigger a #GP.
	 *
	 * This is correct even when VMEXIT rewrites the TSS limit
	 * to 0x67 as the only requirement is that the base points
	 * outside the limit.
	 */
	tss->x86_tss.io_bitmap_base = IO_BITMAP_OFFSET_INVALID;
}