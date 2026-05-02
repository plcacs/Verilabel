static void addECSOption(char* packet, const size_t& packetSize, uint16_t* len, const ComboAddress& remote, int stamp)
{
  string EDNSRR;
  struct dnsheader* dh = (struct dnsheader*) packet;

  EDNSSubnetOpts eso;
  if(stamp < 0)
    eso.source = Netmask(remote);
  else {
    ComboAddress stamped(remote);
    *((char*)&stamped.sin4.sin_addr.s_addr)=stamp;
    eso.source = Netmask(stamped);
  }
  string optRData=makeEDNSSubnetOptsString(eso);
  string record;
  generateEDNSOption(EDNSOptionCode::ECS, optRData, record);
  generateOptRR(record, EDNSRR);


  uint16_t arcount = ntohs(dh->arcount);
  /* does it fit in the existing buffer? */
  if (packetSize - *len > EDNSRR.size()) {
    arcount++;
    dh->arcount = htons(arcount);
    memcpy(packet + *len, EDNSRR.c_str(), EDNSRR.size());
    *len += EDNSRR.size();
  }
}