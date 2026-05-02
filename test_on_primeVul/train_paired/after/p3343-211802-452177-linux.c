int wmi_set_ie(struct wil6210_priv *wil, u8 type, u16 ie_len, const void *ie)
{
	static const char *const names[] = {
		[WMI_FRAME_BEACON]	= "BEACON",
		[WMI_FRAME_PROBE_REQ]	= "PROBE_REQ",
		[WMI_FRAME_PROBE_RESP]	= "WMI_FRAME_PROBE_RESP",
		[WMI_FRAME_ASSOC_REQ]	= "WMI_FRAME_ASSOC_REQ",
		[WMI_FRAME_ASSOC_RESP]	= "WMI_FRAME_ASSOC_RESP",
	};
	int rc;
	u16 len = sizeof(struct wmi_set_appie_cmd) + ie_len;
	struct wmi_set_appie_cmd *cmd;

	if (len < ie_len) {
		rc = -EINVAL;
		goto out;
	}

	cmd = kzalloc(len, GFP_KERNEL);
	if (!cmd) {
		rc = -ENOMEM;
		goto out;
	}
	if (!ie)
		ie_len = 0;

	cmd->mgmt_frm_type = type;
	/* BUG: FW API define ieLen as u8. Will fix FW */
	cmd->ie_len = cpu_to_le16(ie_len);
	memcpy(cmd->ie_info, ie, ie_len);
	rc = wmi_send(wil, WMI_SET_APPIE_CMDID, cmd, len);
	kfree(cmd);
out:
	if (rc) {
		const char *name = type < ARRAY_SIZE(names) ?
				   names[type] : "??";
		wil_err(wil, "set_ie(%d %s) failed : %d\n", type, name, rc);
	}

	return rc;
}