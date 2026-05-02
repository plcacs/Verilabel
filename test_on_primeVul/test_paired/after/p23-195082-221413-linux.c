void recalc_intercepts(struct vcpu_svm *svm)
{
	struct vmcb_control_area *c, *h, *g;
	unsigned int i;

	vmcb_mark_dirty(svm->vmcb, VMCB_INTERCEPTS);

	if (!is_guest_mode(&svm->vcpu))
		return;

	c = &svm->vmcb->control;
	h = &svm->vmcb01.ptr->control;
	g = &svm->nested.ctl;

	for (i = 0; i < MAX_INTERCEPT; i++)
		c->intercepts[i] = h->intercepts[i];

	if (g->int_ctl & V_INTR_MASKING_MASK) {
		/* We only want the cr8 intercept bits of L1 */
		vmcb_clr_intercept(c, INTERCEPT_CR8_READ);
		vmcb_clr_intercept(c, INTERCEPT_CR8_WRITE);

		/*
		 * Once running L2 with HF_VINTR_MASK, EFLAGS.IF does not
		 * affect any interrupt we may want to inject; therefore,
		 * interrupt window vmexits are irrelevant to L0.
		 */
		vmcb_clr_intercept(c, INTERCEPT_VINTR);
	}

	/* We don't want to see VMMCALLs from a nested guest */
	vmcb_clr_intercept(c, INTERCEPT_VMMCALL);

	for (i = 0; i < MAX_INTERCEPT; i++)
		c->intercepts[i] |= g->intercepts[i];

	/* If SMI is not intercepted, ignore guest SMI intercept as well  */
	if (!intercept_smi)
		vmcb_clr_intercept(c, INTERCEPT_SMI);

	vmcb_set_intercept(c, INTERCEPT_VMLOAD);
	vmcb_set_intercept(c, INTERCEPT_VMSAVE);
}