static inline void native_tss_invalidate_io_bitmap(void)
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
	this_cpu_write(cpu_tss_rw.x86_tss.io_bitmap_base,
		       IO_BITMAP_OFFSET_INVALID);
}