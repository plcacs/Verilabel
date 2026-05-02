static struct kvm_vcpu_hv_synic *synic_get(struct kvm *kvm, u32 vpidx)
{
	struct kvm_vcpu *vcpu;
	struct kvm_vcpu_hv_synic *synic;

	vcpu = get_vcpu_by_vpidx(kvm, vpidx);
	if (!vcpu)
		return NULL;
	synic = to_hv_synic(vcpu);
	return (synic->active) ? synic : NULL;
}