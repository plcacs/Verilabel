static int bond_ipsec_add_sa(struct xfrm_state *xs)
{
	struct net_device *bond_dev = xs->xso.dev;
	struct bonding *bond;
	struct slave *slave;
	int err;

	if (!bond_dev)
		return -EINVAL;

	rcu_read_lock();
	bond = netdev_priv(bond_dev);
	slave = rcu_dereference(bond->curr_active_slave);
	if (!slave) {
		rcu_read_unlock();
		return -ENODEV;
	}

	xs->xso.real_dev = slave->dev;
	bond->xs = xs;

	if (!(slave->dev->xfrmdev_ops
	      && slave->dev->xfrmdev_ops->xdo_dev_state_add)) {
		slave_warn(bond_dev, slave->dev, "Slave does not support ipsec offload\n");
		rcu_read_unlock();
		return -EINVAL;
	}

	err = slave->dev->xfrmdev_ops->xdo_dev_state_add(xs);
	rcu_read_unlock();
	return err;
}