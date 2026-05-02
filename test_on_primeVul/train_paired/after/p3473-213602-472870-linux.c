__mptctl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	mpt_ioctl_header __user *uhdr = (void __user *) arg;
	mpt_ioctl_header	 khdr;
	int iocnum;
	unsigned iocnumX;
	int nonblock = (file->f_flags & O_NONBLOCK);
	int ret;
	MPT_ADAPTER *iocp = NULL;

	if (copy_from_user(&khdr, uhdr, sizeof(khdr))) {
		printk(KERN_ERR MYNAM "%s::mptctl_ioctl() @%d - "
				"Unable to copy mpt_ioctl_header data @ %p\n",
				__FILE__, __LINE__, uhdr);
		return -EFAULT;
	}
	ret = -ENXIO;				/* (-6) No such device or address */

	/* Verify intended MPT adapter - set iocnum and the adapter
	 * pointer (iocp)
	 */
	iocnumX = khdr.iocnum & 0xFF;
	if (((iocnum = mpt_verify_adapter(iocnumX, &iocp)) < 0) ||
	    (iocp == NULL))
		return -ENODEV;

	if (!iocp->active) {
		printk(KERN_DEBUG MYNAM "%s::mptctl_ioctl() @%d - Controller disabled.\n",
				__FILE__, __LINE__);
		return -EFAULT;
	}

	/* Handle those commands that are just returning
	 * information stored in the driver.
	 * These commands should never time out and are unaffected
	 * by TM and FW reloads.
	 */
	if ((cmd & ~IOCSIZE_MASK) == (MPTIOCINFO & ~IOCSIZE_MASK)) {
		return mptctl_getiocinfo(iocp, arg, _IOC_SIZE(cmd));
	} else if (cmd == MPTTARGETINFO) {
		return mptctl_gettargetinfo(iocp, arg);
	} else if (cmd == MPTTEST) {
		return mptctl_readtest(iocp, arg);
	} else if (cmd == MPTEVENTQUERY) {
		return mptctl_eventquery(iocp, arg);
	} else if (cmd == MPTEVENTENABLE) {
		return mptctl_eventenable(iocp, arg);
	} else if (cmd == MPTEVENTREPORT) {
		return mptctl_eventreport(iocp, arg);
	} else if (cmd == MPTFWREPLACE) {
		return mptctl_replace_fw(iocp, arg);
	}

	/* All of these commands require an interrupt or
	 * are unknown/illegal.
	 */
	if ((ret = mptctl_syscall_down(iocp, nonblock)) != 0)
		return ret;

	if (cmd == MPTFWDOWNLOAD)
		ret = mptctl_fw_download(iocp, arg);
	else if (cmd == MPTCOMMAND)
		ret = mptctl_mpt_command(iocp, arg);
	else if (cmd == MPTHARDRESET)
		ret = mptctl_do_reset(iocp, arg);
	else if ((cmd & ~IOCSIZE_MASK) == (HP_GETHOSTINFO & ~IOCSIZE_MASK))
		ret = mptctl_hp_hostinfo(iocp, arg, _IOC_SIZE(cmd));
	else if (cmd == HP_GETTARGETINFO)
		ret = mptctl_hp_targetinfo(iocp, arg);
	else
		ret = -EINVAL;

	mutex_unlock(&iocp->ioctl_cmds.mutex);

	return ret;
}