static long acrn_dev_ioctl(struct file *filp, unsigned int cmd,
			   unsigned long ioctl_param)
{
	struct acrn_vm *vm = filp->private_data;
	struct acrn_vm_creation *vm_param;
	struct acrn_vcpu_regs *cpu_regs;
	struct acrn_ioreq_notify notify;
	struct acrn_ptdev_irq *irq_info;
	struct acrn_ioeventfd ioeventfd;
	struct acrn_vm_memmap memmap;
	struct acrn_mmiodev *mmiodev;
	struct acrn_msi_entry *msi;
	struct acrn_pcidev *pcidev;
	struct acrn_irqfd irqfd;
	struct acrn_vdev *vdev;
	struct page *page;
	u64 cstate_cmd;
	int i, ret = 0;

	if (vm->vmid == ACRN_INVALID_VMID && cmd != ACRN_IOCTL_CREATE_VM) {
		dev_dbg(acrn_dev.this_device,
			"ioctl 0x%x: Invalid VM state!\n", cmd);
		return -EINVAL;
	}

	switch (cmd) {
	case ACRN_IOCTL_CREATE_VM:
		vm_param = memdup_user((void __user *)ioctl_param,
				       sizeof(struct acrn_vm_creation));
		if (IS_ERR(vm_param))
			return PTR_ERR(vm_param);

		if ((vm_param->reserved0 | vm_param->reserved1) != 0)
			return -EINVAL;

		vm = acrn_vm_create(vm, vm_param);
		if (!vm) {
			ret = -EINVAL;
			kfree(vm_param);
			break;
		}

		if (copy_to_user((void __user *)ioctl_param, vm_param,
				 sizeof(struct acrn_vm_creation))) {
			acrn_vm_destroy(vm);
			ret = -EFAULT;
		}

		kfree(vm_param);
		break;
	case ACRN_IOCTL_START_VM:
		ret = hcall_start_vm(vm->vmid);
		if (ret < 0)
			dev_dbg(acrn_dev.this_device,
				"Failed to start VM %u!\n", vm->vmid);
		break;
	case ACRN_IOCTL_PAUSE_VM:
		ret = hcall_pause_vm(vm->vmid);
		if (ret < 0)
			dev_dbg(acrn_dev.this_device,
				"Failed to pause VM %u!\n", vm->vmid);
		break;
	case ACRN_IOCTL_RESET_VM:
		ret = hcall_reset_vm(vm->vmid);
		if (ret < 0)
			dev_dbg(acrn_dev.this_device,
				"Failed to restart VM %u!\n", vm->vmid);
		break;
	case ACRN_IOCTL_DESTROY_VM:
		ret = acrn_vm_destroy(vm);
		break;
	case ACRN_IOCTL_SET_VCPU_REGS:
		cpu_regs = memdup_user((void __user *)ioctl_param,
				       sizeof(struct acrn_vcpu_regs));
		if (IS_ERR(cpu_regs))
			return PTR_ERR(cpu_regs);

		for (i = 0; i < ARRAY_SIZE(cpu_regs->reserved); i++)
			if (cpu_regs->reserved[i])
				return -EINVAL;

		for (i = 0; i < ARRAY_SIZE(cpu_regs->vcpu_regs.reserved_32); i++)
			if (cpu_regs->vcpu_regs.reserved_32[i])
				return -EINVAL;

		for (i = 0; i < ARRAY_SIZE(cpu_regs->vcpu_regs.reserved_64); i++)
			if (cpu_regs->vcpu_regs.reserved_64[i])
				return -EINVAL;

		for (i = 0; i < ARRAY_SIZE(cpu_regs->vcpu_regs.gdt.reserved); i++)
			if (cpu_regs->vcpu_regs.gdt.reserved[i] |
			    cpu_regs->vcpu_regs.idt.reserved[i])
				return -EINVAL;

		ret = hcall_set_vcpu_regs(vm->vmid, virt_to_phys(cpu_regs));
		if (ret < 0)
			dev_dbg(acrn_dev.this_device,
				"Failed to set regs state of VM%u!\n",
				vm->vmid);
		kfree(cpu_regs);
		break;
	case ACRN_IOCTL_SET_MEMSEG:
		if (copy_from_user(&memmap, (void __user *)ioctl_param,
				   sizeof(memmap)))
			return -EFAULT;

		ret = acrn_vm_memseg_map(vm, &memmap);
		break;
	case ACRN_IOCTL_UNSET_MEMSEG:
		if (copy_from_user(&memmap, (void __user *)ioctl_param,
				   sizeof(memmap)))
			return -EFAULT;

		ret = acrn_vm_memseg_unmap(vm, &memmap);
		break;
	case ACRN_IOCTL_ASSIGN_MMIODEV:
		mmiodev = memdup_user((void __user *)ioctl_param,
				      sizeof(struct acrn_mmiodev));
		if (IS_ERR(mmiodev))
			return PTR_ERR(mmiodev);

		ret = hcall_assign_mmiodev(vm->vmid, virt_to_phys(mmiodev));
		if (ret < 0)
			dev_dbg(acrn_dev.this_device,
				"Failed to assign MMIO device!\n");
		kfree(mmiodev);
		break;
	case ACRN_IOCTL_DEASSIGN_MMIODEV:
		mmiodev = memdup_user((void __user *)ioctl_param,
				      sizeof(struct acrn_mmiodev));
		if (IS_ERR(mmiodev))
			return PTR_ERR(mmiodev);

		ret = hcall_deassign_mmiodev(vm->vmid, virt_to_phys(mmiodev));
		if (ret < 0)
			dev_dbg(acrn_dev.this_device,
				"Failed to deassign MMIO device!\n");
		kfree(mmiodev);
		break;
	case ACRN_IOCTL_ASSIGN_PCIDEV:
		pcidev = memdup_user((void __user *)ioctl_param,
				     sizeof(struct acrn_pcidev));
		if (IS_ERR(pcidev))
			return PTR_ERR(pcidev);

		ret = hcall_assign_pcidev(vm->vmid, virt_to_phys(pcidev));
		if (ret < 0)
			dev_dbg(acrn_dev.this_device,
				"Failed to assign pci device!\n");
		kfree(pcidev);
		break;
	case ACRN_IOCTL_DEASSIGN_PCIDEV:
		pcidev = memdup_user((void __user *)ioctl_param,
				     sizeof(struct acrn_pcidev));
		if (IS_ERR(pcidev))
			return PTR_ERR(pcidev);

		ret = hcall_deassign_pcidev(vm->vmid, virt_to_phys(pcidev));
		if (ret < 0)
			dev_dbg(acrn_dev.this_device,
				"Failed to deassign pci device!\n");
		kfree(pcidev);
		break;
	case ACRN_IOCTL_CREATE_VDEV:
		vdev = memdup_user((void __user *)ioctl_param,
				   sizeof(struct acrn_vdev));
		if (IS_ERR(vdev))
			return PTR_ERR(vdev);

		ret = hcall_create_vdev(vm->vmid, virt_to_phys(vdev));
		if (ret < 0)
			dev_dbg(acrn_dev.this_device,
				"Failed to create virtual device!\n");
		kfree(vdev);
		break;
	case ACRN_IOCTL_DESTROY_VDEV:
		vdev = memdup_user((void __user *)ioctl_param,
				   sizeof(struct acrn_vdev));
		if (IS_ERR(vdev))
			return PTR_ERR(vdev);
		ret = hcall_destroy_vdev(vm->vmid, virt_to_phys(vdev));
		if (ret < 0)
			dev_dbg(acrn_dev.this_device,
				"Failed to destroy virtual device!\n");
		kfree(vdev);
		break;
	case ACRN_IOCTL_SET_PTDEV_INTR:
		irq_info = memdup_user((void __user *)ioctl_param,
				       sizeof(struct acrn_ptdev_irq));
		if (IS_ERR(irq_info))
			return PTR_ERR(irq_info);

		ret = hcall_set_ptdev_intr(vm->vmid, virt_to_phys(irq_info));
		if (ret < 0)
			dev_dbg(acrn_dev.this_device,
				"Failed to configure intr for ptdev!\n");
		kfree(irq_info);
		break;
	case ACRN_IOCTL_RESET_PTDEV_INTR:
		irq_info = memdup_user((void __user *)ioctl_param,
				       sizeof(struct acrn_ptdev_irq));
		if (IS_ERR(irq_info))
			return PTR_ERR(irq_info);

		ret = hcall_reset_ptdev_intr(vm->vmid, virt_to_phys(irq_info));
		if (ret < 0)
			dev_dbg(acrn_dev.this_device,
				"Failed to reset intr for ptdev!\n");
		kfree(irq_info);
		break;
	case ACRN_IOCTL_SET_IRQLINE:
		ret = hcall_set_irqline(vm->vmid, ioctl_param);
		if (ret < 0)
			dev_dbg(acrn_dev.this_device,
				"Failed to set interrupt line!\n");
		break;
	case ACRN_IOCTL_INJECT_MSI:
		msi = memdup_user((void __user *)ioctl_param,
				  sizeof(struct acrn_msi_entry));
		if (IS_ERR(msi))
			return PTR_ERR(msi);

		ret = hcall_inject_msi(vm->vmid, virt_to_phys(msi));
		if (ret < 0)
			dev_dbg(acrn_dev.this_device,
				"Failed to inject MSI!\n");
		kfree(msi);
		break;
	case ACRN_IOCTL_VM_INTR_MONITOR:
		ret = pin_user_pages_fast(ioctl_param, 1,
					  FOLL_WRITE | FOLL_LONGTERM, &page);
		if (unlikely(ret != 1)) {
			dev_dbg(acrn_dev.this_device,
				"Failed to pin intr hdr buffer!\n");
			return -EFAULT;
		}

		ret = hcall_vm_intr_monitor(vm->vmid, page_to_phys(page));
		if (ret < 0) {
			unpin_user_page(page);
			dev_dbg(acrn_dev.this_device,
				"Failed to monitor intr data!\n");
			return ret;
		}
		if (vm->monitor_page)
			unpin_user_page(vm->monitor_page);
		vm->monitor_page = page;
		break;
	case ACRN_IOCTL_CREATE_IOREQ_CLIENT:
		if (vm->default_client)
			return -EEXIST;
		if (!acrn_ioreq_client_create(vm, NULL, NULL, true, "acrndm"))
			ret = -EINVAL;
		break;
	case ACRN_IOCTL_DESTROY_IOREQ_CLIENT:
		if (vm->default_client)
			acrn_ioreq_client_destroy(vm->default_client);
		break;
	case ACRN_IOCTL_ATTACH_IOREQ_CLIENT:
		if (vm->default_client)
			ret = acrn_ioreq_client_wait(vm->default_client);
		else
			ret = -ENODEV;
		break;
	case ACRN_IOCTL_NOTIFY_REQUEST_FINISH:
		if (copy_from_user(&notify, (void __user *)ioctl_param,
				   sizeof(struct acrn_ioreq_notify)))
			return -EFAULT;

		if (notify.reserved != 0)
			return -EINVAL;

		ret = acrn_ioreq_request_default_complete(vm, notify.vcpu);
		break;
	case ACRN_IOCTL_CLEAR_VM_IOREQ:
		acrn_ioreq_request_clear(vm);
		break;
	case ACRN_IOCTL_PM_GET_CPU_STATE:
		if (copy_from_user(&cstate_cmd, (void __user *)ioctl_param,
				   sizeof(cstate_cmd)))
			return -EFAULT;

		ret = pmcmd_ioctl(cstate_cmd, (void __user *)ioctl_param);
		break;
	case ACRN_IOCTL_IOEVENTFD:
		if (copy_from_user(&ioeventfd, (void __user *)ioctl_param,
				   sizeof(ioeventfd)))
			return -EFAULT;

		if (ioeventfd.reserved != 0)
			return -EINVAL;

		ret = acrn_ioeventfd_config(vm, &ioeventfd);
		break;
	case ACRN_IOCTL_IRQFD:
		if (copy_from_user(&irqfd, (void __user *)ioctl_param,
				   sizeof(irqfd)))
			return -EFAULT;
		ret = acrn_irqfd_config(vm, &irqfd);
		break;
	default:
		dev_dbg(acrn_dev.this_device, "Unknown IOCTL 0x%x!\n", cmd);
		ret = -ENOTTY;
	}

	return ret;
}