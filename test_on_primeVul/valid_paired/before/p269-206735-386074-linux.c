static int rtsx_usb_ms_drv_remove(struct platform_device *pdev)
{
	struct rtsx_usb_ms *host = platform_get_drvdata(pdev);
	struct memstick_host *msh = host->msh;
	int err;

	host->eject = true;
	cancel_work_sync(&host->handle_req);

	mutex_lock(&host->host_mutex);
	if (host->req) {
		dev_dbg(ms_dev(host),
			"%s: Controller removed during transfer\n",
			dev_name(&msh->dev));
		host->req->error = -ENOMEDIUM;
		do {
			err = memstick_next_req(msh, &host->req);
			if (!err)
				host->req->error = -ENOMEDIUM;
		} while (!err);
	}
	mutex_unlock(&host->host_mutex);

	memstick_remove_host(msh);
	memstick_free_host(msh);

	/* Balance possible unbalanced usage count
	 * e.g. unconditional module removal
	 */
	if (pm_runtime_active(ms_dev(host)))
		pm_runtime_put(ms_dev(host));

	pm_runtime_disable(ms_dev(host));
	platform_set_drvdata(pdev, NULL);

	dev_dbg(ms_dev(host),
		": Realtek USB Memstick controller has been removed\n");

	return 0;
}