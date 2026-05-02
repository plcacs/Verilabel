static void host_callback(void *arg, int status, int timeouts,
                          unsigned char *abuf, int alen)
{
  struct host_query *hquery = (struct host_query*)arg;
  int addinfostatus = ARES_SUCCESS;
  hquery->timeouts += timeouts;
  hquery->remaining--;

  if (status == ARES_SUCCESS)
    {
      addinfostatus = ares__parse_into_addrinfo(abuf, alen, hquery->ai);
    }
  else if (status == ARES_EDESTRUCTION)
    {
      end_hquery(hquery, status);
    }

  if (!hquery->remaining)
    {
      if (addinfostatus != ARES_SUCCESS)
        {
          /* error in parsing result e.g. no memory */
          end_hquery(hquery, addinfostatus);
        }
      else if (hquery->ai->nodes)
        {
          /* at least one query ended with ARES_SUCCESS */
          end_hquery(hquery, ARES_SUCCESS);
        }
      else if (status == ARES_ENOTFOUND)
        {
          next_lookup(hquery, status);
        }
      else
        {
          end_hquery(hquery, status);
        }
    }

  /* at this point we keep on waiting for the next query to finish */
}