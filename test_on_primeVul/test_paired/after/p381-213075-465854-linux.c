void nfcmrvl_nci_unregister_dev(struct nfcmrvl_private *priv)
{
	struct nci_dev *ndev = priv->ndev;

	nci_unregister_device(ndev);
	if (priv->ndev->nfc_dev->fw_download_in_progress)
		nfcmrvl_fw_dnld_abort(priv);

	nfcmrvl_fw_dnld_deinit(priv);

	if (gpio_is_valid(priv->config.reset_n_io))
		gpio_free(priv->config.reset_n_io);

	nci_free_device(ndev);
	kfree(priv);
}