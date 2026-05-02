net_bind(short unsigned *port, int type, const char *log_service_name)
{
  struct addrinfo hints = { 0 };
  struct addrinfo *servinfo;
  struct addrinfo *ptr;
  const char *cfgaddr;
  char addr[INET6_ADDRSTRLEN];
  char strport[8];
  int yes = 1;
  int no = 0;
  int fd;
  int ret;

  cfgaddr = cfg_getstr(cfg_getsec(cfg, "general"), "bind_address");

  hints.ai_socktype = (type & (SOCK_STREAM | SOCK_DGRAM)); // filter since type can be SOCK_STREAM | SOCK_NONBLOCK
  hints.ai_family = (cfg_getbool(cfg_getsec(cfg, "general"), "ipv6")) ? AF_INET6 : AF_INET;
  hints.ai_flags = cfgaddr ? 0 : AI_PASSIVE;

  snprintf(strport, sizeof(strport), "%hu", *port);
  ret = getaddrinfo(cfgaddr, strport, &hints, &servinfo);
  if (ret < 0)
    {
      DPRINTF(E_LOG, L_MISC, "Failure creating '%s' service, could not resolve '%s' (port %s): %s\n", log_service_name, cfgaddr ? cfgaddr : "(ANY)", strport, gai_strerror(ret));
      return -1;
    }

  for (ptr = servinfo, fd = -1; ptr != NULL; ptr = ptr->ai_next)
    {
      if (fd >= 0)
	close(fd);

      fd = socket(ptr->ai_family, type | SOCK_CLOEXEC, ptr->ai_protocol);
      if (fd < 0)
	continue;

      // TODO libevent sets this, we do the same?
      ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
      if (ret < 0)
	continue;

      ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
      if (ret < 0)
	continue;

      if (ptr->ai_family == AF_INET6)
	{
	  // We want to be sure the service is dual stack
	  ret = setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
	  if (ret < 0)
	    continue;
	}

      ret = bind(fd, ptr->ai_addr, ptr->ai_addrlen);
      if (ret < 0)
	continue;

      break;
    }

  freeaddrinfo(servinfo);

  if (!ptr)
    {
      DPRINTF(E_LOG, L_MISC, "Could not create service '%s' with address %s, port %hu: %s\n", log_service_name, cfgaddr ? cfgaddr : "(ANY)", *port, strerror(errno));
      goto error;
    }

  // Get the port that was assigned
  ret = getsockname(fd, ptr->ai_addr, &ptr->ai_addrlen);
  if (ret < 0)
    {
      DPRINTF(E_LOG, L_MISC, "Could not find address of service '%s': %s\n", log_service_name, strerror(errno));
      goto error;
    }

  net_port_get(port, (union net_sockaddr *)ptr->ai_addr);
  net_address_get(addr, sizeof(addr), (union net_sockaddr *)ptr->ai_addr);

  DPRINTF(E_DBG, L_MISC, "Service '%s' bound to %s, port %hu, socket %d\n", log_service_name, addr, *port, fd);

  return fd;

 error:
  close(fd);
  return -1;
}