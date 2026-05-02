static inline bool kvm_apic_has_events(struct kvm_vcpu *vcpu)
{
	return vcpu->arch.apic->pending_events;
}