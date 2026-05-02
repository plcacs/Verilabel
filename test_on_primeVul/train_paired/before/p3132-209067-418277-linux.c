static int rtw_wx_set_scan(struct net_device *dev, struct iw_request_info *a,
			   union iwreq_data *wrqu, char *extra)
{
	u8 _status = false;
	int ret = 0;
	struct adapter *padapter = rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	struct ndis_802_11_ssid ssid[RTW_SSID_SCAN_AMOUNT];

	RT_TRACE(_module_rtl871x_mlme_c_, _drv_info_, ("%s\n", __func__));

	if (!rtw_pwr_wakeup(padapter)) {
		ret = -1;
		goto exit;
	}

	if (padapter->bDriverStopped) {
		DBG_88E("bDriverStopped =%d\n", padapter->bDriverStopped);
		ret = -1;
		goto exit;
	}

	if (!padapter->bup) {
		ret = -1;
		goto exit;
	}

	if (!padapter->hw_init_completed) {
		ret = -1;
		goto exit;
	}

	/*  When Busy Traffic, driver do not site survey. So driver return success. */
	/*  wpa_supplicant will not issue SIOCSIWSCAN cmd again after scan timeout. */
	/*  modify by thomas 2011-02-22. */
	if (pmlmepriv->LinkDetectInfo.bBusyTraffic) {
		indicate_wx_scan_complete_event(padapter);
		goto exit;
	}

	if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY | _FW_UNDER_LINKING)) {
		indicate_wx_scan_complete_event(padapter);
		goto exit;
	}

/*	For the DMP WiFi Display project, the driver won't to scan because */
/*	the pmlmepriv->scan_interval is always equal to 3. */
/*	So, the wpa_supplicant won't find out the WPS SoftAP. */

	memset(ssid, 0, sizeof(struct ndis_802_11_ssid) * RTW_SSID_SCAN_AMOUNT);

	if (wrqu->data.length == sizeof(struct iw_scan_req)) {
		struct iw_scan_req *req = (struct iw_scan_req *)extra;

		if (wrqu->data.flags & IW_SCAN_THIS_ESSID) {
			int len = min_t(int, req->essid_len,
					IW_ESSID_MAX_SIZE);

			memcpy(ssid[0].ssid, req->essid, len);
			ssid[0].ssid_length = len;

			DBG_88E("IW_SCAN_THIS_ESSID, ssid =%s, len =%d\n", req->essid, req->essid_len);

			spin_lock_bh(&pmlmepriv->lock);

			_status = rtw_sitesurvey_cmd(padapter, ssid, 1, NULL, 0);

			spin_unlock_bh(&pmlmepriv->lock);
		} else if (req->scan_type == IW_SCAN_TYPE_PASSIVE) {
			DBG_88E("%s, req->scan_type == IW_SCAN_TYPE_PASSIVE\n", __func__);
		}
	} else {
		if (wrqu->data.length >= WEXT_CSCAN_HEADER_SIZE &&
		    !memcmp(extra, WEXT_CSCAN_HEADER, WEXT_CSCAN_HEADER_SIZE)) {
			int len = wrqu->data.length - WEXT_CSCAN_HEADER_SIZE;
			char *pos = extra + WEXT_CSCAN_HEADER_SIZE;
			char section;
			char sec_len;
			int ssid_index = 0;

			while (len >= 1) {
				section = *(pos++);
				len -= 1;

				switch (section) {
				case WEXT_CSCAN_SSID_SECTION:
					if (len < 1) {
						len = 0;
						break;
					}
					sec_len = *(pos++); len -= 1;
					if (sec_len > 0 && sec_len <= len) {
						ssid[ssid_index].ssid_length = sec_len;
						memcpy(ssid[ssid_index].ssid, pos, ssid[ssid_index].ssid_length);
						ssid_index++;
					}
					pos += sec_len;
					len -= sec_len;
					break;
				case WEXT_CSCAN_TYPE_SECTION:
				case WEXT_CSCAN_CHANNEL_SECTION:
					pos += 1;
					len -= 1;
					break;
				case WEXT_CSCAN_PASV_DWELL_SECTION:
				case WEXT_CSCAN_HOME_DWELL_SECTION:
				case WEXT_CSCAN_ACTV_DWELL_SECTION:
					pos += 2;
					len -= 2;
					break;
				default:
					len = 0; /*  stop parsing */
				}
			}

			/* it has still some scan parameter to parse, we only do this now... */
			_status = rtw_set_802_11_bssid_list_scan(padapter, ssid, RTW_SSID_SCAN_AMOUNT);
		} else {
			_status = rtw_set_802_11_bssid_list_scan(padapter, NULL, 0);
		}
	}

	if (!_status)
		ret = -1;

exit:

	return ret;
}