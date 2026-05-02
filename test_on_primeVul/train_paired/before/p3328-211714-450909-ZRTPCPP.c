void ZRtp::storeMsgTemp(ZrtpPacketBase* pkt) {
    int32_t length = pkt->getLength() * ZRTP_WORD_SIZE;
    memset(tempMsgBuffer, 0, sizeof(tempMsgBuffer));
    memcpy(tempMsgBuffer, (uint8_t*)pkt->getHeaderBase(), length);
    lengthOfMsgData = length;
}