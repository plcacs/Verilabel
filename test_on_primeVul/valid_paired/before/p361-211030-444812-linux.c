mwifiex_cmd_802_11_ad_hoc_start(struct mwifiex_private *priv,
				struct host_cmd_ds_command *cmd,
				struct cfg80211_ssid *req_ssid)
{
	int rsn_ie_len = 0;
	struct mwifiex_adapter *adapter = priv->adapter;
	struct host_cmd_ds_802_11_ad_hoc_start *adhoc_start =
		&cmd->params.adhoc_start;
	struct mwifiex_bssdescriptor *bss_desc;
	u32 cmd_append_size = 0;
	u32 i;
	u16 tmp_cap;
	struct mwifiex_ie_types_chan_list_param_set *chan_tlv;
	u8 radio_type;

	struct mwifiex_ie_types_htcap *ht_cap;
	struct mwifiex_ie_types_htinfo *ht_info;
	u8 *pos = (u8 *) adhoc_start +
			sizeof(struct host_cmd_ds_802_11_ad_hoc_start);

	if (!adapter)
		return -1;

	cmd->command = cpu_to_le16(HostCmd_CMD_802_11_AD_HOC_START);

	bss_desc = &priv->curr_bss_params.bss_descriptor;
	priv->attempted_bss_desc = bss_desc;

	/*
	 * Fill in the parameters for 2 data structures:
	 *   1. struct host_cmd_ds_802_11_ad_hoc_start command
	 *   2. bss_desc
	 * Driver will fill up SSID, bss_mode,IBSS param, Physical Param,
	 * probe delay, and Cap info.
	 * Firmware will fill up beacon period, Basic rates
	 * and operational rates.
	 */

	memset(adhoc_start->ssid, 0, IEEE80211_MAX_SSID_LEN);

	memcpy(adhoc_start->ssid, req_ssid->ssid, req_ssid->ssid_len);

	mwifiex_dbg(adapter, INFO, "info: ADHOC_S_CMD: SSID = %s\n",
		    adhoc_start->ssid);

	memset(bss_desc->ssid.ssid, 0, IEEE80211_MAX_SSID_LEN);
	memcpy(bss_desc->ssid.ssid, req_ssid->ssid, req_ssid->ssid_len);

	bss_desc->ssid.ssid_len = req_ssid->ssid_len;

	/* Set the BSS mode */
	adhoc_start->bss_mode = HostCmd_BSS_MODE_IBSS;
	bss_desc->bss_mode = NL80211_IFTYPE_ADHOC;
	adhoc_start->beacon_period = cpu_to_le16(priv->beacon_period);
	bss_desc->beacon_period = priv->beacon_period;

	/* Set Physical param set */
/* Parameter IE Id */
#define DS_PARA_IE_ID   3
/* Parameter IE length */
#define DS_PARA_IE_LEN  1

	adhoc_start->phy_param_set.ds_param_set.element_id = DS_PARA_IE_ID;
	adhoc_start->phy_param_set.ds_param_set.len = DS_PARA_IE_LEN;

	if (!mwifiex_get_cfp(priv, adapter->adhoc_start_band,
			     (u16) priv->adhoc_channel, 0)) {
		struct mwifiex_chan_freq_power *cfp;
		cfp = mwifiex_get_cfp(priv, adapter->adhoc_start_band,
				      FIRST_VALID_CHANNEL, 0);
		if (cfp)
			priv->adhoc_channel = (u8) cfp->channel;
	}

	if (!priv->adhoc_channel) {
		mwifiex_dbg(adapter, ERROR,
			    "ADHOC_S_CMD: adhoc_channel cannot be 0\n");
		return -1;
	}

	mwifiex_dbg(adapter, INFO,
		    "info: ADHOC_S_CMD: creating ADHOC on channel %d\n",
		    priv->adhoc_channel);

	priv->curr_bss_params.bss_descriptor.channel = priv->adhoc_channel;
	priv->curr_bss_params.band = adapter->adhoc_start_band;

	bss_desc->channel = priv->adhoc_channel;
	adhoc_start->phy_param_set.ds_param_set.current_chan =
		priv->adhoc_channel;

	memcpy(&bss_desc->phy_param_set, &adhoc_start->phy_param_set,
	       sizeof(union ieee_types_phy_param_set));

	/* Set IBSS param set */
/* IBSS parameter IE Id */
#define IBSS_PARA_IE_ID   6
/* IBSS parameter IE length */
#define IBSS_PARA_IE_LEN  2

	adhoc_start->ss_param_set.ibss_param_set.element_id = IBSS_PARA_IE_ID;
	adhoc_start->ss_param_set.ibss_param_set.len = IBSS_PARA_IE_LEN;
	adhoc_start->ss_param_set.ibss_param_set.atim_window
					= cpu_to_le16(priv->atim_window);
	memcpy(&bss_desc->ss_param_set, &adhoc_start->ss_param_set,
	       sizeof(union ieee_types_ss_param_set));

	/* Set Capability info */
	bss_desc->cap_info_bitmap |= WLAN_CAPABILITY_IBSS;
	tmp_cap = WLAN_CAPABILITY_IBSS;

	/* Set up privacy in bss_desc */
	if (priv->sec_info.encryption_mode) {
		/* Ad-Hoc capability privacy on */
		mwifiex_dbg(adapter, INFO,
			    "info: ADHOC_S_CMD: wep_status set privacy to WEP\n");
		bss_desc->privacy = MWIFIEX_802_11_PRIV_FILTER_8021X_WEP;
		tmp_cap |= WLAN_CAPABILITY_PRIVACY;
	} else {
		mwifiex_dbg(adapter, INFO,
			    "info: ADHOC_S_CMD: wep_status NOT set,\t"
			    "setting privacy to ACCEPT ALL\n");
		bss_desc->privacy = MWIFIEX_802_11_PRIV_FILTER_ACCEPT_ALL;
	}

	memset(adhoc_start->data_rate, 0, sizeof(adhoc_start->data_rate));
	mwifiex_get_active_data_rates(priv, adhoc_start->data_rate);
	if ((adapter->adhoc_start_band & BAND_G) &&
	    (priv->curr_pkt_filter & HostCmd_ACT_MAC_ADHOC_G_PROTECTION_ON)) {
		if (mwifiex_send_cmd(priv, HostCmd_CMD_MAC_CONTROL,
				     HostCmd_ACT_GEN_SET, 0,
				     &priv->curr_pkt_filter, false)) {
			mwifiex_dbg(adapter, ERROR,
				    "ADHOC_S_CMD: G Protection config failed\n");
			return -1;
		}
	}
	/* Find the last non zero */
	for (i = 0; i < sizeof(adhoc_start->data_rate); i++)
		if (!adhoc_start->data_rate[i])
			break;

	priv->curr_bss_params.num_of_rates = i;

	/* Copy the ad-hoc creating rates into Current BSS rate structure */
	memcpy(&priv->curr_bss_params.data_rates,
	       &adhoc_start->data_rate, priv->curr_bss_params.num_of_rates);

	mwifiex_dbg(adapter, INFO, "info: ADHOC_S_CMD: rates=%4ph\n",
		    adhoc_start->data_rate);

	mwifiex_dbg(adapter, INFO, "info: ADHOC_S_CMD: AD-HOC Start command is ready\n");

	if (IS_SUPPORT_MULTI_BANDS(adapter)) {
		/* Append a channel TLV */
		chan_tlv = (struct mwifiex_ie_types_chan_list_param_set *) pos;
		chan_tlv->header.type = cpu_to_le16(TLV_TYPE_CHANLIST);
		chan_tlv->header.len =
			cpu_to_le16(sizeof(struct mwifiex_chan_scan_param_set));

		memset(chan_tlv->chan_scan_param, 0x00,
		       sizeof(struct mwifiex_chan_scan_param_set));
		chan_tlv->chan_scan_param[0].chan_number =
			(u8) priv->curr_bss_params.bss_descriptor.channel;

		mwifiex_dbg(adapter, INFO, "info: ADHOC_S_CMD: TLV Chan = %d\n",
			    chan_tlv->chan_scan_param[0].chan_number);

		chan_tlv->chan_scan_param[0].radio_type
		       = mwifiex_band_to_radio_type(priv->curr_bss_params.band);
		if (adapter->adhoc_start_band & BAND_GN ||
		    adapter->adhoc_start_band & BAND_AN) {
			if (adapter->sec_chan_offset ==
					    IEEE80211_HT_PARAM_CHA_SEC_ABOVE)
				chan_tlv->chan_scan_param[0].radio_type |=
					(IEEE80211_HT_PARAM_CHA_SEC_ABOVE << 4);
			else if (adapter->sec_chan_offset ==
					    IEEE80211_HT_PARAM_CHA_SEC_BELOW)
				chan_tlv->chan_scan_param[0].radio_type |=
					(IEEE80211_HT_PARAM_CHA_SEC_BELOW << 4);
		}
		mwifiex_dbg(adapter, INFO, "info: ADHOC_S_CMD: TLV Band = %d\n",
			    chan_tlv->chan_scan_param[0].radio_type);
		pos += sizeof(chan_tlv->header) +
			sizeof(struct mwifiex_chan_scan_param_set);
		cmd_append_size +=
			sizeof(chan_tlv->header) +
			sizeof(struct mwifiex_chan_scan_param_set);
	}

	/* Append vendor specific IE TLV */
	cmd_append_size += mwifiex_cmd_append_vsie_tlv(priv,
				MWIFIEX_VSIE_MASK_ADHOC, &pos);

	if (priv->sec_info.wpa_enabled) {
		rsn_ie_len = mwifiex_append_rsn_ie_wpa_wpa2(priv, &pos);
		if (rsn_ie_len == -1)
			return -1;
		cmd_append_size += rsn_ie_len;
	}

	if (adapter->adhoc_11n_enabled) {
		/* Fill HT CAPABILITY */
		ht_cap = (struct mwifiex_ie_types_htcap *) pos;
		memset(ht_cap, 0, sizeof(struct mwifiex_ie_types_htcap));
		ht_cap->header.type = cpu_to_le16(WLAN_EID_HT_CAPABILITY);
		ht_cap->header.len =
		       cpu_to_le16(sizeof(struct ieee80211_ht_cap));
		radio_type = mwifiex_band_to_radio_type(
					priv->adapter->config_bands);
		mwifiex_fill_cap_info(priv, radio_type, &ht_cap->ht_cap);

		if (adapter->sec_chan_offset ==
					IEEE80211_HT_PARAM_CHA_SEC_NONE) {
			u16 tmp_ht_cap;

			tmp_ht_cap = le16_to_cpu(ht_cap->ht_cap.cap_info);
			tmp_ht_cap &= ~IEEE80211_HT_CAP_SUP_WIDTH_20_40;
			tmp_ht_cap &= ~IEEE80211_HT_CAP_SGI_40;
			ht_cap->ht_cap.cap_info = cpu_to_le16(tmp_ht_cap);
		}

		pos += sizeof(struct mwifiex_ie_types_htcap);
		cmd_append_size += sizeof(struct mwifiex_ie_types_htcap);

		/* Fill HT INFORMATION */
		ht_info = (struct mwifiex_ie_types_htinfo *) pos;
		memset(ht_info, 0, sizeof(struct mwifiex_ie_types_htinfo));
		ht_info->header.type = cpu_to_le16(WLAN_EID_HT_OPERATION);
		ht_info->header.len =
			cpu_to_le16(sizeof(struct ieee80211_ht_operation));

		ht_info->ht_oper.primary_chan =
			(u8) priv->curr_bss_params.bss_descriptor.channel;
		if (adapter->sec_chan_offset) {
			ht_info->ht_oper.ht_param = adapter->sec_chan_offset;
			ht_info->ht_oper.ht_param |=
					IEEE80211_HT_PARAM_CHAN_WIDTH_ANY;
		}
		ht_info->ht_oper.operation_mode =
		     cpu_to_le16(IEEE80211_HT_OP_MODE_NON_GF_STA_PRSNT);
		ht_info->ht_oper.basic_set[0] = 0xff;
		pos += sizeof(struct mwifiex_ie_types_htinfo);
		cmd_append_size +=
				sizeof(struct mwifiex_ie_types_htinfo);
	}

	cmd->size =
		cpu_to_le16((u16)(sizeof(struct host_cmd_ds_802_11_ad_hoc_start)
				  + S_DS_GEN + cmd_append_size));

	if (adapter->adhoc_start_band == BAND_B)
		tmp_cap &= ~WLAN_CAPABILITY_SHORT_SLOT_TIME;
	else
		tmp_cap |= WLAN_CAPABILITY_SHORT_SLOT_TIME;

	adhoc_start->cap_info_bitmap = cpu_to_le16(tmp_cap);

	return 0;
}