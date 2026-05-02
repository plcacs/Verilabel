uint8_t ethereum_extractThorchainData(const EthereumSignTx *msg,
                                          char *buffer) {
  // Swap data begins 164 chars into data buffer:
  // offset = deposit function hash + address + address + uint256
  uint16_t offset = 4 + (5 * 32);
  int16_t len = msg->data_length - offset;
  if (msg->has_data_length && len > 0 && len < 256) {
    memcpy(buffer, msg->data_initial_chunk.bytes + offset, len);
    // String length must be < 255 characters
    return (uint8_t)len;
  }
  return 0;
}