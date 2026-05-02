static int dccp_setsockopt_change(struct sock *sk, int type,
				  struct dccp_so_feat __user *optval)
{
	struct dccp_so_feat opt;
	u8 *val;
	int rc;

	if (copy_from_user(&opt, optval, sizeof(opt)))
		return -EFAULT;

	val = kmalloc(opt.dccpsf_len, GFP_KERNEL);
	if (!val)
		return -ENOMEM;

	if (copy_from_user(val, opt.dccpsf_val, opt.dccpsf_len)) {
		rc = -EFAULT;
		goto out_free_val;
	}

	rc = dccp_feat_change(dccp_msk(sk), type, opt.dccpsf_feat,
			      val, opt.dccpsf_len, GFP_KERNEL);
	if (rc)
		goto out_free_val;

out:
	return rc;

out_free_val:
	kfree(val);
	goto out;
}