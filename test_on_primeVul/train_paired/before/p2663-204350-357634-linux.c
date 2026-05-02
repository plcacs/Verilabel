static void vcpu_scan_ioapic(struct kvm_vcpu *vcpu)
{
	if (!kvm_apic_hw_enabled(vcpu->arch.apic))
		return;

	bitmap_zero(vcpu->arch.ioapic_handled_vectors, 256);

	if (irqchip_split(vcpu->kvm))
		kvm_scan_ioapic_routes(vcpu, vcpu->arch.ioapic_handled_vectors);
	else {
		if (vcpu->arch.apicv_active)
			kvm_x86_ops->sync_pir_to_irr(vcpu);
		kvm_ioapic_scan_entry(vcpu, vcpu->arch.ioapic_handled_vectors);
	}

	if (is_guest_mode(vcpu))
		vcpu->arch.load_eoi_exitmap_pending = true;
	else
		kvm_make_request(KVM_REQ_LOAD_EOI_EXITMAP, vcpu);
}