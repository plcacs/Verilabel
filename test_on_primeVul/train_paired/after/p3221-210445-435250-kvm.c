static int do_insn_fetch(struct x86_emulate_ctxt *ctxt,
			 struct x86_emulate_ops *ops,
			 unsigned long eip, void *dest, unsigned size)
{
	int rc = 0;

	/* x86 instructions are limited to 15 bytes. */
	if (eip + size - ctxt->decode.eip_orig > 15)
		return X86EMUL_UNHANDLEABLE;
	eip += ctxt->cs_base;
	while (size--) {
		rc = do_fetch_insn_byte(ctxt, ops, eip++, dest++);
		if (rc)
			return rc;
	}
	return 0;
}