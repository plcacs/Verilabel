static inline bool kvm_apic_has_events(struct kvm_vcpu *vcpu)
{
	return kvm_vcpu_has_lapic(vcpu) && vcpu->arch.apic->pending_events;
}