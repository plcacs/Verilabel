int wvlan_set_netname(struct net_device *dev,
		      struct iw_request_info *info,
		      union iwreq_data *wrqu,
		      char *extra)
{
	struct wl_private *lp = wl_priv(dev);
	unsigned long flags;
	int ret = 0;
	/*------------------------------------------------------------------------*/


	DBG_FUNC("wvlan_set_netname");
	DBG_ENTER(DbgInfo);

	wl_lock(lp, &flags);

	memset(lp->NetworkName, 0, sizeof(lp->NetworkName));
	memcpy(lp->NetworkName, extra, wrqu->data.length);

	/* Commit the adapter parameters */
	wl_apply(lp);
	wl_unlock(lp, &flags);

	DBG_LEAVE(DbgInfo);
	return ret;
} /* wvlan_set_netname */
