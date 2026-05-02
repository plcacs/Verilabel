static int svm_cpu_init(int cpu)
{
	struct svm_cpu_data *sd;
	int r;

	sd = kzalloc(sizeof(struct svm_cpu_data), GFP_KERNEL);
	if (!sd)
		return -ENOMEM;
	sd->cpu = cpu;
	r = -ENOMEM;
	sd->save_area = alloc_page(GFP_KERNEL);
	if (!sd->save_area)
		goto err_1;

	if (svm_sev_enabled()) {
		r = -ENOMEM;
		sd->sev_vmcbs = kmalloc_array(max_sev_asid + 1,
					      sizeof(void *),
					      GFP_KERNEL);
		if (!sd->sev_vmcbs)
			goto err_1;
	}

	per_cpu(svm_data, cpu) = sd;

	return 0;

err_1:
	kfree(sd);
	return r;

}