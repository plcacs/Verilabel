static BOOL autodetect_recv_bandwidth_measure_results(rdpRdp* rdp, wStream* s,
                                                      AUTODETECT_RSP_PDU* autodetectRspPdu)
{
	BOOL success = TRUE;

	if (autodetectRspPdu->headerLength != 0x0E)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "received Bandwidth Measure Results PDU");
	Stream_Read_UINT32(s, rdp->autodetect->bandwidthMeasureTimeDelta); /* timeDelta (4 bytes) */
	Stream_Read_UINT32(s, rdp->autodetect->bandwidthMeasureByteCount); /* byteCount (4 bytes) */

	if (rdp->autodetect->bandwidthMeasureTimeDelta > 0)
		rdp->autodetect->netCharBandwidth = rdp->autodetect->bandwidthMeasureByteCount * 8 /
		                                    rdp->autodetect->bandwidthMeasureTimeDelta;
	else
		rdp->autodetect->netCharBandwidth = 0;

	IFCALLRET(rdp->autodetect->BandwidthMeasureResults, success, rdp->context,
	          autodetectRspPdu->sequenceNumber);
	return success;
}