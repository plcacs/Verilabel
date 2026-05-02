static void rtrs_clt_dev_release(struct device *dev)
{
	struct rtrs_clt_sess *clt = container_of(dev, struct rtrs_clt_sess,
						 dev);

	kfree(clt);
}