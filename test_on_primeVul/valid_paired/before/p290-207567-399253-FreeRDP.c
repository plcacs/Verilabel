BOOL license_read_new_or_upgrade_license_packet(rdpLicense* license, wStream* s)
{
	UINT32 os_major;
	UINT32 os_minor;
	UINT32 cbScope, cbCompanyName, cbProductId, cbLicenseInfo;
	wStream* licenseStream = NULL;
	BOOL ret = FALSE;
	BYTE computedMac[16];
	LICENSE_BLOB* calBlob;

	DEBUG_LICENSE("Receiving Server New/Upgrade License Packet");

	calBlob = license_new_binary_blob(BB_DATA_BLOB);
	if (!calBlob)
		return FALSE;

	/* EncryptedLicenseInfo */
	if (!license_read_encrypted_blob(license, s, calBlob))
		goto out_free_blob;

	/* compute MAC and check it */
	if (Stream_GetRemainingLength(s) < 16)
		goto out_free_blob;

	if (!security_mac_data(license->MacSaltKey, calBlob->data, calBlob->length, computedMac))
		goto out_free_blob;

	if (memcmp(computedMac, Stream_Pointer(s), sizeof(computedMac)) != 0)
	{
		WLog_ERR(TAG, "new or upgrade license MAC mismatch");
		goto out_free_blob;
	}

	if (!Stream_SafeSeek(s, 16))
		goto out_free_blob;

	licenseStream = Stream_New(calBlob->data, calBlob->length);
	if (!licenseStream)
		goto out_free_blob;

	Stream_Read_UINT16(licenseStream, os_minor);
	Stream_Read_UINT16(licenseStream, os_major);

	/* Scope */
	Stream_Read_UINT32(licenseStream, cbScope);
	if (Stream_GetRemainingLength(licenseStream) < cbScope)
		goto out_free_stream;
#ifdef WITH_DEBUG_LICENSE
	WLog_DBG(TAG, "Scope:");
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Pointer(licenseStream), cbScope);
#endif
	Stream_Seek(licenseStream, cbScope);

	/* CompanyName */
	Stream_Read_UINT32(licenseStream, cbCompanyName);
	if (Stream_GetRemainingLength(licenseStream) < cbCompanyName)
		goto out_free_stream;
#ifdef WITH_DEBUG_LICENSE
	WLog_DBG(TAG, "Company name:");
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Pointer(licenseStream), cbCompanyName);
#endif
	Stream_Seek(licenseStream, cbCompanyName);

	/* productId */
	Stream_Read_UINT32(licenseStream, cbProductId);
	if (Stream_GetRemainingLength(licenseStream) < cbProductId)
		goto out_free_stream;
#ifdef WITH_DEBUG_LICENSE
	WLog_DBG(TAG, "Product id:");
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Pointer(licenseStream), cbProductId);
#endif
	Stream_Seek(licenseStream, cbProductId);

	/* licenseInfo */
	Stream_Read_UINT32(licenseStream, cbLicenseInfo);
	if (Stream_GetRemainingLength(licenseStream) < cbLicenseInfo)
		goto out_free_stream;

	license->state = LICENSE_STATE_COMPLETED;

	ret = TRUE;
	if (!license->rdp->settings->OldLicenseBehaviour)
		ret = saveCal(license->rdp->settings, Stream_Pointer(licenseStream), cbLicenseInfo,
		              license->rdp->settings->ClientHostname);

out_free_stream:
	Stream_Free(licenseStream, FALSE);
out_free_blob:
	license_free_binary_blob(calBlob);
	return ret;
}