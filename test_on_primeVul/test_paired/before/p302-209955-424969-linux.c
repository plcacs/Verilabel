struct iwl_trans *iwl_trans_pcie_alloc(struct pci_dev *pdev,
			       const struct pci_device_id *ent,
			       const struct iwl_cfg_trans_params *cfg_trans)
{
	struct iwl_trans_pcie *trans_pcie;
	struct iwl_trans *trans;
	int ret, addr_size;

	ret = pcim_enable_device(pdev);
	if (ret)
		return ERR_PTR(ret);

	if (cfg_trans->gen2)
		trans = iwl_trans_alloc(sizeof(struct iwl_trans_pcie),
					&pdev->dev, &trans_ops_pcie_gen2);
	else
		trans = iwl_trans_alloc(sizeof(struct iwl_trans_pcie),
					&pdev->dev, &trans_ops_pcie);

	if (!trans)
		return ERR_PTR(-ENOMEM);

	trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);

	trans_pcie->trans = trans;
	trans_pcie->opmode_down = true;
	spin_lock_init(&trans_pcie->irq_lock);
	spin_lock_init(&trans_pcie->reg_lock);
	mutex_init(&trans_pcie->mutex);
	init_waitqueue_head(&trans_pcie->ucode_write_waitq);
	trans_pcie->tso_hdr_page = alloc_percpu(struct iwl_tso_hdr_page);
	if (!trans_pcie->tso_hdr_page) {
		ret = -ENOMEM;
		goto out_no_pci;
	}
	trans_pcie->debug_rfkill = -1;

	if (!cfg_trans->base_params->pcie_l1_allowed) {
		/*
		 * W/A - seems to solve weird behavior. We need to remove this
		 * if we don't want to stay in L1 all the time. This wastes a
		 * lot of power.
		 */
		pci_disable_link_state(pdev, PCIE_LINK_STATE_L0S |
				       PCIE_LINK_STATE_L1 |
				       PCIE_LINK_STATE_CLKPM);
	}

	trans_pcie->def_rx_queue = 0;

	if (cfg_trans->use_tfh) {
		addr_size = 64;
		trans_pcie->max_tbs = IWL_TFH_NUM_TBS;
		trans_pcie->tfd_size = sizeof(struct iwl_tfh_tfd);
	} else {
		addr_size = 36;
		trans_pcie->max_tbs = IWL_NUM_OF_TBS;
		trans_pcie->tfd_size = sizeof(struct iwl_tfd);
	}
	trans->max_skb_frags = IWL_PCIE_MAX_FRAGS(trans_pcie);

	pci_set_master(pdev);

	ret = pci_set_dma_mask(pdev, DMA_BIT_MASK(addr_size));
	if (!ret)
		ret = pci_set_consistent_dma_mask(pdev,
						  DMA_BIT_MASK(addr_size));
	if (ret) {
		ret = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (!ret)
			ret = pci_set_consistent_dma_mask(pdev,
							  DMA_BIT_MASK(32));
		/* both attempts failed: */
		if (ret) {
			dev_err(&pdev->dev, "No suitable DMA available\n");
			goto out_no_pci;
		}
	}

	ret = pcim_iomap_regions_request_all(pdev, BIT(0), DRV_NAME);
	if (ret) {
		dev_err(&pdev->dev, "pcim_iomap_regions_request_all failed\n");
		goto out_no_pci;
	}

	trans_pcie->hw_base = pcim_iomap_table(pdev)[0];
	if (!trans_pcie->hw_base) {
		dev_err(&pdev->dev, "pcim_iomap_table failed\n");
		ret = -ENODEV;
		goto out_no_pci;
	}

	/* We disable the RETRY_TIMEOUT register (0x41) to keep
	 * PCI Tx retries from interfering with C3 CPU state */
	pci_write_config_byte(pdev, PCI_CFG_RETRY_TIMEOUT, 0x00);

	trans_pcie->pci_dev = pdev;
	iwl_disable_interrupts(trans);

	trans->hw_rev = iwl_read32(trans, CSR_HW_REV);
	if (trans->hw_rev == 0xffffffff) {
		dev_err(&pdev->dev, "HW_REV=0xFFFFFFFF, PCI issues?\n");
		ret = -EIO;
		goto out_no_pci;
	}

	/*
	 * In the 8000 HW family the format of the 4 bytes of CSR_HW_REV have
	 * changed, and now the revision step also includes bit 0-1 (no more
	 * "dash" value). To keep hw_rev backwards compatible - we'll store it
	 * in the old format.
	 */
	if (cfg_trans->device_family >= IWL_DEVICE_FAMILY_8000) {
		trans->hw_rev = (trans->hw_rev & 0xfff0) |
				(CSR_HW_REV_STEP(trans->hw_rev << 2) << 2);

		ret = iwl_pcie_prepare_card_hw(trans);
		if (ret) {
			IWL_WARN(trans, "Exit HW not ready\n");
			goto out_no_pci;
		}

		/*
		 * in-order to recognize C step driver should read chip version
		 * id located at the AUX bus MISC address space.
		 */
		ret = iwl_finish_nic_init(trans, cfg_trans);
		if (ret)
			goto out_no_pci;

	}

	IWL_DEBUG_INFO(trans, "HW REV: 0x%0x\n", trans->hw_rev);

	iwl_pcie_set_interrupt_capa(pdev, trans, cfg_trans);
	trans->hw_id = (pdev->device << 16) + pdev->subsystem_device;
	snprintf(trans->hw_id_str, sizeof(trans->hw_id_str),
		 "PCI ID: 0x%04X:0x%04X", pdev->device, pdev->subsystem_device);

	/* Initialize the wait queue for commands */
	init_waitqueue_head(&trans_pcie->wait_command_queue);

	init_waitqueue_head(&trans_pcie->sx_waitq);

	if (trans_pcie->msix_enabled) {
		ret = iwl_pcie_init_msix_handler(pdev, trans_pcie);
		if (ret)
			goto out_no_pci;
	 } else {
		ret = iwl_pcie_alloc_ict(trans);
		if (ret)
			goto out_no_pci;

		ret = devm_request_threaded_irq(&pdev->dev, pdev->irq,
						iwl_pcie_isr,
						iwl_pcie_irq_handler,
						IRQF_SHARED, DRV_NAME, trans);
		if (ret) {
			IWL_ERR(trans, "Error allocating IRQ %d\n", pdev->irq);
			goto out_free_ict;
		}
		trans_pcie->inta_mask = CSR_INI_SET_MASK;
	 }

	trans_pcie->rba.alloc_wq = alloc_workqueue("rb_allocator",
						   WQ_HIGHPRI | WQ_UNBOUND, 1);
	INIT_WORK(&trans_pcie->rba.rx_alloc, iwl_pcie_rx_allocator_work);

#ifdef CONFIG_IWLWIFI_DEBUGFS
	trans_pcie->fw_mon_data.state = IWL_FW_MON_DBGFS_STATE_CLOSED;
	mutex_init(&trans_pcie->fw_mon_data.mutex);
#endif

	return trans;

out_free_ict:
	iwl_pcie_free_ict(trans);
out_no_pci:
	free_percpu(trans_pcie->tso_hdr_page);
	iwl_trans_free(trans);
	return ERR_PTR(ret);
}