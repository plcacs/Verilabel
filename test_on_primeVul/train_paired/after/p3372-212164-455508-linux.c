static bool svm_need_emulation_on_page_fault(struct kvm_vcpu *vcpu)
{
	unsigned long cr4 = kvm_read_cr4(vcpu);
	bool smep = cr4 & X86_CR4_SMEP;
	bool smap = cr4 & X86_CR4_SMAP;
	bool is_user = svm_get_cpl(vcpu) == 3;

	/*
	 * If RIP is invalid, go ahead with emulation which will cause an
	 * internal error exit.
	 */
	if (!kvm_vcpu_gfn_to_memslot(vcpu, kvm_rip_read(vcpu) >> PAGE_SHIFT))
		return true;

	/*
	 * Detect and workaround Errata 1096 Fam_17h_00_0Fh.
	 *
	 * Errata:
	 * When CPU raise #NPF on guest data access and vCPU CR4.SMAP=1, it is
	 * possible that CPU microcode implementing DecodeAssist will fail
	 * to read bytes of instruction which caused #NPF. In this case,
	 * GuestIntrBytes field of the VMCB on a VMEXIT will incorrectly
	 * return 0 instead of the correct guest instruction bytes.
	 *
	 * This happens because CPU microcode reading instruction bytes
	 * uses a special opcode which attempts to read data using CPL=0
	 * priviledges. The microcode reads CS:RIP and if it hits a SMAP
	 * fault, it gives up and returns no instruction bytes.
	 *
	 * Detection:
	 * We reach here in case CPU supports DecodeAssist, raised #NPF and
	 * returned 0 in GuestIntrBytes field of the VMCB.
	 * First, errata can only be triggered in case vCPU CR4.SMAP=1.
	 * Second, if vCPU CR4.SMEP=1, errata could only be triggered
	 * in case vCPU CPL==3 (Because otherwise guest would have triggered
	 * a SMEP fault instead of #NPF).
	 * Otherwise, vCPU CR4.SMEP=0, errata could be triggered by any vCPU CPL.
	 * As most guests enable SMAP if they have also enabled SMEP, use above
	 * logic in order to attempt minimize false-positive of detecting errata
	 * while still preserving all cases semantic correctness.
	 *
	 * Workaround:
	 * To determine what instruction the guest was executing, the hypervisor
	 * will have to decode the instruction at the instruction pointer.
	 *
	 * In non SEV guest, hypervisor will be able to read the guest
	 * memory to decode the instruction pointer when insn_len is zero
	 * so we return true to indicate that decoding is possible.
	 *
	 * But in the SEV guest, the guest memory is encrypted with the
	 * guest specific key and hypervisor will not be able to decode the
	 * instruction pointer so we will not able to workaround it. Lets
	 * print the error and request to kill the guest.
	 */
	if (smap && (!smep || is_user)) {
		if (!sev_guest(vcpu->kvm))
			return true;

		pr_err_ratelimited("KVM: SEV Guest triggered AMD Erratum 1096\n");
		kvm_make_request(KVM_REQ_TRIPLE_FAULT, vcpu);
	}

	return false;
}