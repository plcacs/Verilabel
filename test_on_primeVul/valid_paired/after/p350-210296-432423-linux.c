static int svm_cpu_init(int cpu)
{
	struct svm_cpu_data *sd;

	sd = kzalloc(sizeof(struct svm_cpu_data), GFP_KERNEL);
	if (!sd)
		return -ENOMEM;
	sd->cpu = cpu;
	sd->save_area = alloc_page(GFP_KERNEL);
	if (!sd->save_area)
		goto free_cpu_data;

	if (svm_sev_enabled()) {
		sd->sev_vmcbs = kmalloc_array(max_sev_asid + 1,
					      sizeof(void *),
					      GFP_KERNEL);
		if (!sd->sev_vmcbs)
			goto free_save_area;
	}

	per_cpu(svm_data, cpu) = sd;

	return 0;

free_save_area:
	__free_page(sd->save_area);
free_cpu_data:
	kfree(sd);
	return -ENOMEM;

}