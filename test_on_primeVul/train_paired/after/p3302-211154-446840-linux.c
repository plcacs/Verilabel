static int yam_siocdevprivate(struct net_device *dev, struct ifreq *ifr, void __user *data, int cmd)
{
	struct yam_port *yp = netdev_priv(dev);
	struct yamdrv_ioctl_cfg yi;
	struct yamdrv_ioctl_mcs *ym;
	int ioctl_cmd;

	if (copy_from_user(&ioctl_cmd, data, sizeof(int)))
		return -EFAULT;

	if (yp->magic != YAM_MAGIC)
		return -EINVAL;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (cmd != SIOCDEVPRIVATE)
		return -EINVAL;

	switch (ioctl_cmd) {

	case SIOCYAMRESERVED:
		return -EINVAL;			/* unused */

	case SIOCYAMSMCS:
		if (netif_running(dev))
			return -EINVAL;		/* Cannot change this parameter when up */
		ym = memdup_user(data, sizeof(struct yamdrv_ioctl_mcs));
		if (IS_ERR(ym))
			return PTR_ERR(ym);
		if (ym->cmd != SIOCYAMSMCS || ym->bitrate > YAM_MAXBITRATE) {
			kfree(ym);
			return -EINVAL;
		}
		/* setting predef as 0 for loading userdefined mcs data */
		add_mcs(ym->bits, ym->bitrate, 0);
		kfree(ym);
		break;

	case SIOCYAMSCFG:
		if (!capable(CAP_SYS_RAWIO))
			return -EPERM;
		if (copy_from_user(&yi, data, sizeof(struct yamdrv_ioctl_cfg)))
			return -EFAULT;

		if (yi.cmd != SIOCYAMSCFG)
			return -EINVAL;
		if ((yi.cfg.mask & YAM_IOBASE) && netif_running(dev))
			return -EINVAL;		/* Cannot change this parameter when up */
		if ((yi.cfg.mask & YAM_IRQ) && netif_running(dev))
			return -EINVAL;		/* Cannot change this parameter when up */
		if ((yi.cfg.mask & YAM_BITRATE) && netif_running(dev))
			return -EINVAL;		/* Cannot change this parameter when up */
		if ((yi.cfg.mask & YAM_BAUDRATE) && netif_running(dev))
			return -EINVAL;		/* Cannot change this parameter when up */

		if (yi.cfg.mask & YAM_IOBASE) {
			yp->iobase = yi.cfg.iobase;
			dev->base_addr = yi.cfg.iobase;
		}
		if (yi.cfg.mask & YAM_IRQ) {
			if (yi.cfg.irq > 15)
				return -EINVAL;
			yp->irq = yi.cfg.irq;
			dev->irq = yi.cfg.irq;
		}
		if (yi.cfg.mask & YAM_BITRATE) {
			if (yi.cfg.bitrate > YAM_MAXBITRATE)
				return -EINVAL;
			yp->bitrate = yi.cfg.bitrate;
		}
		if (yi.cfg.mask & YAM_BAUDRATE) {
			if (yi.cfg.baudrate > YAM_MAXBAUDRATE)
				return -EINVAL;
			yp->baudrate = yi.cfg.baudrate;
		}
		if (yi.cfg.mask & YAM_MODE) {
			if (yi.cfg.mode > YAM_MAXMODE)
				return -EINVAL;
			yp->dupmode = yi.cfg.mode;
		}
		if (yi.cfg.mask & YAM_HOLDDLY) {
			if (yi.cfg.holddly > YAM_MAXHOLDDLY)
				return -EINVAL;
			yp->holdd = yi.cfg.holddly;
		}
		if (yi.cfg.mask & YAM_TXDELAY) {
			if (yi.cfg.txdelay > YAM_MAXTXDELAY)
				return -EINVAL;
			yp->txd = yi.cfg.txdelay;
		}
		if (yi.cfg.mask & YAM_TXTAIL) {
			if (yi.cfg.txtail > YAM_MAXTXTAIL)
				return -EINVAL;
			yp->txtail = yi.cfg.txtail;
		}
		if (yi.cfg.mask & YAM_PERSIST) {
			if (yi.cfg.persist > YAM_MAXPERSIST)
				return -EINVAL;
			yp->pers = yi.cfg.persist;
		}
		if (yi.cfg.mask & YAM_SLOTTIME) {
			if (yi.cfg.slottime > YAM_MAXSLOTTIME)
				return -EINVAL;
			yp->slot = yi.cfg.slottime;
			yp->slotcnt = yp->slot / 10;
		}
		break;

	case SIOCYAMGCFG:
		memset(&yi, 0, sizeof(yi));
		yi.cfg.mask = 0xffffffff;
		yi.cfg.iobase = yp->iobase;
		yi.cfg.irq = yp->irq;
		yi.cfg.bitrate = yp->bitrate;
		yi.cfg.baudrate = yp->baudrate;
		yi.cfg.mode = yp->dupmode;
		yi.cfg.txdelay = yp->txd;
		yi.cfg.holddly = yp->holdd;
		yi.cfg.txtail = yp->txtail;
		yi.cfg.persist = yp->pers;
		yi.cfg.slottime = yp->slot;
		if (copy_to_user(data, &yi, sizeof(struct yamdrv_ioctl_cfg)))
			return -EFAULT;
		break;

	default:
		return -EINVAL;

	}

	return 0;
}