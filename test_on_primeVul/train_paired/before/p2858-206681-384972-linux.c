static inline int init_new_context(struct task_struct *tsk,
				   struct mm_struct *mm)
{
	#ifdef CONFIG_X86_INTEL_MEMORY_PROTECTION_KEYS
	if (cpu_feature_enabled(X86_FEATURE_OSPKE)) {
		/* pkey 0 is the default and always allocated */
		mm->context.pkey_allocation_map = 0x1;
		/* -1 means unallocated or invalid */
		mm->context.execute_only_pkey = -1;
	}
	#endif
	init_new_context_ldt(tsk, mm);

	return 0;
}