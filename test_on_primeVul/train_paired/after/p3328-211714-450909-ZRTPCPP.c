void ZRtp::storeMsgTemp(ZrtpPacketBase* pkt) {
    uint32_t length = pkt->getLength() * ZRTP_WORD_SIZE;
    length = (length > sizeof(tempMsgBuffer)) ? sizeof(tempMsgBuffer) : length;
    memset(tempMsgBuffer, 0, sizeof(tempMsgBuffer));
    memcpy(tempMsgBuffer, (uint8_t*)pkt->getHeaderBase(), length);
    lengthOfMsgData = length;
}