int detach_capi_ctr(struct capi_ctr *ctr)
{
	int err = 0;

	mutex_lock(&capi_controller_lock);

	ctr_down(ctr, CAPI_CTR_DETACHED);

	if (capi_controller[ctr->cnr - 1] != ctr) {
		err = -EINVAL;
		goto unlock_out;
	}
	capi_controller[ctr->cnr - 1] = NULL;
	ncontrollers--;

	if (ctr->procent)
		remove_proc_entry(ctr->procfn, NULL);

	printk(KERN_NOTICE "kcapi: controller [%03d]: %s unregistered\n",
	       ctr->cnr, ctr->name);

unlock_out:
	mutex_unlock(&capi_controller_lock);

	return err;
}