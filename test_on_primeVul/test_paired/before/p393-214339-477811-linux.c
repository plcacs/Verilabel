int kvmppc_rtas_hcall(struct kvm_vcpu *vcpu)
{
	struct rtas_token_definition *d;
	struct rtas_args args;
	rtas_arg_t *orig_rets;
	gpa_t args_phys;
	int rc;

	/*
	 * r4 contains the guest physical address of the RTAS args
	 * Mask off the top 4 bits since this is a guest real address
	 */
	args_phys = kvmppc_get_gpr(vcpu, 4) & KVM_PAM;

	vcpu->srcu_idx = srcu_read_lock(&vcpu->kvm->srcu);
	rc = kvm_read_guest(vcpu->kvm, args_phys, &args, sizeof(args));
	srcu_read_unlock(&vcpu->kvm->srcu, vcpu->srcu_idx);
	if (rc)
		goto fail;

	/*
	 * args->rets is a pointer into args->args. Now that we've
	 * copied args we need to fix it up to point into our copy,
	 * not the guest args. We also need to save the original
	 * value so we can restore it on the way out.
	 */
	orig_rets = args.rets;
	args.rets = &args.args[be32_to_cpu(args.nargs)];

	mutex_lock(&vcpu->kvm->arch.rtas_token_lock);

	rc = -ENOENT;
	list_for_each_entry(d, &vcpu->kvm->arch.rtas_tokens, list) {
		if (d->token == be32_to_cpu(args.token)) {
			d->handler->handler(vcpu, &args);
			rc = 0;
			break;
		}
	}

	mutex_unlock(&vcpu->kvm->arch.rtas_token_lock);

	if (rc == 0) {
		args.rets = orig_rets;
		rc = kvm_write_guest(vcpu->kvm, args_phys, &args, sizeof(args));
		if (rc)
			goto fail;
	}

	return rc;

fail:
	/*
	 * We only get here if the guest has called RTAS with a bogus
	 * args pointer. That means we can't get to the args, and so we
	 * can't fail the RTAS call. So fail right out to userspace,
	 * which should kill the guest.
	 */
	return rc;
}