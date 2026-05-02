static void rtrs_clt_dev_release(struct device *dev)
{
	struct rtrs_clt_sess *clt = container_of(dev, struct rtrs_clt_sess,
						 dev);

	mutex_destroy(&clt->paths_ev_mutex);
	mutex_destroy(&clt->paths_mutex);
	kfree(clt);
}