grub_password_get (char buf[], unsigned buf_size)
{
  unsigned cur_len = 0;
  int key;

  while (1)
    {
      key = grub_getkey (); 
      if (key == '\n' || key == '\r')
	break;

      if (key == '\e')
	{
	  cur_len = 0;
	  break;
	}

      if (key == '\b')
	{
	  if (cur_len)
	    cur_len--;
	  continue;
	}

      if (!grub_isprint (key))
	continue;

      if (cur_len + 2 < buf_size)
	buf[cur_len++] = key;
    }

  grub_memset (buf + cur_len, 0, buf_size - cur_len);

  grub_xputs ("\n");
  grub_refresh ();

  return (key != '\e');
}