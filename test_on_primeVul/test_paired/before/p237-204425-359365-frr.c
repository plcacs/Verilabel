bgp_capability_msg_parse (struct peer *peer, u_char *pnt, bgp_size_t length)
{
  u_char *end;
  struct capability cap;
  u_char action;
  struct bgp *bgp;
  afi_t afi;
  safi_t safi;

  bgp = peer->bgp;
  end = pnt + length;

  while (pnt < end)
    {
      /* We need at least action, capability code and capability length. */
      if (pnt + 3 > end)
        {
          zlog_info ("%s Capability length error", peer->host);
          bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
          return -1;
        }

      action = *pnt;

      /* Fetch structure to the byte stream. */
      memcpy (&cap, pnt + 1, sizeof (struct capability));

      /* Action value check.  */
      if (action != CAPABILITY_ACTION_SET
	  && action != CAPABILITY_ACTION_UNSET)
        {
          zlog_info ("%s Capability Action Value error %d",
		     peer->host, action);
          bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
          return -1;
        }

      if (BGP_DEBUG (normal, NORMAL))
	zlog_debug ("%s CAPABILITY has action: %d, code: %u, length %u",
		   peer->host, action, cap.code, cap.length);

      /* Capability length check. */
      if (pnt + (cap.length + 3) > end)
        {
          zlog_info ("%s Capability length error", peer->host);
          bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
          return -1;
        }

      /* We know MP Capability Code. */
      if (cap.code == CAPABILITY_CODE_MP)
        {
	  afi = ntohs (cap.mpc.afi);
	  safi = cap.mpc.safi;

          /* Ignore capability when override-capability is set. */
          if (CHECK_FLAG (peer->flags, PEER_FLAG_OVERRIDE_CAPABILITY))
	    continue;

	  /* Address family check.  */
	  if ((afi == AFI_IP 
	       || afi == AFI_IP6)
	      && (safi == SAFI_UNICAST 
		  || safi == SAFI_MULTICAST 
		  || safi == BGP_SAFI_VPNV4))
	    {
	      if (BGP_DEBUG (normal, NORMAL))
		zlog_debug ("%s CAPABILITY has %s MP_EXT CAP for afi/safi: %u/%u",
			   peer->host,
			   action == CAPABILITY_ACTION_SET 
			   ? "Advertising" : "Removing",
			   ntohs(cap.mpc.afi) , cap.mpc.safi);
		  
	      /* Adjust safi code. */
	      if (safi == BGP_SAFI_VPNV4)
		safi = SAFI_MPLS_VPN;
	      
	      if (action == CAPABILITY_ACTION_SET)
		{
		  peer->afc_recv[afi][safi] = 1;
		  if (peer->afc[afi][safi])
		    {
		      peer->afc_nego[afi][safi] = 1;
		      bgp_announce_route (peer, afi, safi);
		    }
		}
	      else
		{
		  peer->afc_recv[afi][safi] = 0;
		  peer->afc_nego[afi][safi] = 0;

		  if (peer_active_nego (peer))
		    bgp_clear_route (peer, afi, safi);
		  else
		    BGP_EVENT_ADD (peer, BGP_Stop);
		} 
	    }
        }
      else
        {
          zlog_warn ("%s unrecognized capability code: %d - ignored",
                     peer->host, cap.code);
        }
      pnt += cap.length + 3;
    }
  return 0;
}