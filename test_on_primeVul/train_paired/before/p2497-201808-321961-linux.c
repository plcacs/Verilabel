static int hgcm_call_preprocess_linaddr(
	const struct vmmdev_hgcm_function_parameter *src_parm,
	void **bounce_buf_ret, size_t *extra)
{
	void *buf, *bounce_buf;
	bool copy_in;
	u32 len;
	int ret;

	buf = (void *)src_parm->u.pointer.u.linear_addr;
	len = src_parm->u.pointer.size;
	copy_in = src_parm->type != VMMDEV_HGCM_PARM_TYPE_LINADDR_OUT;

	if (len > VBG_MAX_HGCM_USER_PARM)
		return -E2BIG;

	bounce_buf = kvmalloc(len, GFP_KERNEL);
	if (!bounce_buf)
		return -ENOMEM;

	if (copy_in) {
		ret = copy_from_user(bounce_buf, (void __user *)buf, len);
		if (ret)
			return -EFAULT;
	} else {
		memset(bounce_buf, 0, len);
	}

	*bounce_buf_ret = bounce_buf;
	hgcm_call_add_pagelist_size(bounce_buf, len, extra);
	return 0;
}