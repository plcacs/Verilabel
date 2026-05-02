video_usercopy(struct file *file, unsigned int orig_cmd, unsigned long arg,
	       v4l2_kioctl func)
{
	char	sbuf[128];
	void    *mbuf = NULL;
	void	*parg = (void *)arg;
	long	err  = -EINVAL;
	bool	has_array_args;
	bool	always_copy = false;
	size_t  array_size = 0;
	void __user *user_ptr = NULL;
	void	**kernel_ptr = NULL;
	unsigned int cmd = video_translate_cmd(orig_cmd);
	const size_t ioc_size = _IOC_SIZE(cmd);

	/*  Copy arguments into temp kernel buffer  */
	if (_IOC_DIR(cmd) != _IOC_NONE) {
		if (ioc_size <= sizeof(sbuf)) {
			parg = sbuf;
		} else {
			/* too big to allocate from stack */
			mbuf = kvmalloc(ioc_size, GFP_KERNEL);
			if (NULL == mbuf)
				return -ENOMEM;
			parg = mbuf;
		}

		err = video_get_user((void __user *)arg, parg, cmd,
				     orig_cmd, &always_copy);
		if (err)
			goto out;
	}

	err = check_array_args(cmd, parg, &array_size, &user_ptr, &kernel_ptr);
	if (err < 0)
		goto out;
	has_array_args = err;

	if (has_array_args) {
		/*
		 * When adding new types of array args, make sure that the
		 * parent argument to ioctl (which contains the pointer to the
		 * array) fits into sbuf (so that mbuf will still remain
		 * unused up to here).
		 */
		mbuf = kvmalloc(array_size, GFP_KERNEL);
		err = -ENOMEM;
		if (NULL == mbuf)
			goto out_array_args;
		err = -EFAULT;
		if (in_compat_syscall())
			err = v4l2_compat_get_array_args(file, mbuf, user_ptr,
							 array_size, orig_cmd,
							 parg);
		else
			err = copy_from_user(mbuf, user_ptr, array_size) ?
								-EFAULT : 0;
		if (err)
			goto out_array_args;
		*kernel_ptr = mbuf;
	}

	/* Handles IOCTL */
	err = func(file, cmd, parg);
	if (err == -ENOTTY || err == -ENOIOCTLCMD) {
		err = -ENOTTY;
		goto out;
	}

	if (err == 0) {
		if (cmd == VIDIOC_DQBUF)
			trace_v4l2_dqbuf(video_devdata(file)->minor, parg);
		else if (cmd == VIDIOC_QBUF)
			trace_v4l2_qbuf(video_devdata(file)->minor, parg);
	}

	if (has_array_args) {
		*kernel_ptr = (void __force *)user_ptr;
		if (in_compat_syscall()) {
			int put_err;

			put_err = v4l2_compat_put_array_args(file, user_ptr, mbuf,
							     array_size, orig_cmd,
							     parg);
			if (put_err)
				err = put_err;
		} else if (copy_to_user(user_ptr, mbuf, array_size)) {
			err = -EFAULT;
		}
		goto out_array_args;
	}
	/*
	 * Some ioctls can return an error, but still have valid
	 * results that must be returned.
	 */
	if (err < 0 && !always_copy)
		goto out;

out_array_args:
	if (video_put_user((void __user *)arg, parg, cmd, orig_cmd))
		err = -EFAULT;
out:
	kvfree(mbuf);
	return err;
}