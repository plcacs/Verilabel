static struct rpmsg_device *rpmsg_virtio_add_ctrl_dev(struct virtio_device *vdev)
{
	struct virtproc_info *vrp = vdev->priv;
	struct virtio_rpmsg_channel *vch;
	struct rpmsg_device *rpdev_ctrl;
	int err = 0;

	vch = kzalloc(sizeof(*vch), GFP_KERNEL);
	if (!vch)
		return ERR_PTR(-ENOMEM);

	/* Link the channel to the vrp */
	vch->vrp = vrp;

	/* Assign public information to the rpmsg_device */
	rpdev_ctrl = &vch->rpdev;
	rpdev_ctrl->ops = &virtio_rpmsg_ops;

	rpdev_ctrl->dev.parent = &vrp->vdev->dev;
	rpdev_ctrl->dev.release = virtio_rpmsg_release_device;
	rpdev_ctrl->little_endian = virtio_is_little_endian(vrp->vdev);

	err = rpmsg_ctrldev_register_device(rpdev_ctrl);
	if (err) {
		kfree(vch);
		return ERR_PTR(err);
	}

	return rpdev_ctrl;
}