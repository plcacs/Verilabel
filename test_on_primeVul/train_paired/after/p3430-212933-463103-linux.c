int cfg80211_mgd_wext_giwessid(struct net_device *dev,
			       struct iw_request_info *info,
			       struct iw_point *data, char *ssid)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	int ret = 0;

	/* call only for station! */
	if (WARN_ON(wdev->iftype != NL80211_IFTYPE_STATION))
		return -EINVAL;

	data->flags = 0;

	wdev_lock(wdev);
	if (wdev->current_bss) {
		const u8 *ie;

		rcu_read_lock();
		ie = ieee80211_bss_get_ie(&wdev->current_bss->pub,
					  WLAN_EID_SSID);
		if (ie) {
			data->flags = 1;
			data->length = ie[1];
			if (data->length > IW_ESSID_MAX_SIZE)
				ret = -EINVAL;
			else
				memcpy(ssid, ie + 2, data->length);
		}
		rcu_read_unlock();
	} else if (wdev->wext.connect.ssid && wdev->wext.connect.ssid_len) {
		data->flags = 1;
		data->length = wdev->wext.connect.ssid_len;
		memcpy(ssid, wdev->wext.connect.ssid, data->length);
	}
	wdev_unlock(wdev);

	return ret;
}