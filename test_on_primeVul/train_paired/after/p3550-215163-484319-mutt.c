ADDRESS *rfc822_parse_adrlist (ADDRESS *top, const char *s)
{
  int ws_pending, nl;
#ifdef EXACT_ADDRESS
  const char *begin;
#endif
  const char *ps;
  char comment[LONG_STRING], phrase[LONG_STRING];
  size_t phraselen = 0, commentlen = 0;
  ADDRESS *cur, *last = NULL;

  RFC822Error = 0;

  last = top;
  while (last && last->next)
    last = last->next;

  ws_pending = is_email_wsp (*s);
  if ((nl = mutt_strlen (s)))
    nl = s[nl - 1] == '\n';

  s = skip_email_wsp(s);
#ifdef EXACT_ADDRESS
  begin = s;
#endif
  while (*s)
  {
    if (*s == ',')
    {
      if (phraselen)
      {
	terminate_buffer (phrase, phraselen);
	add_addrspec (&top, &last, phrase, comment, &commentlen, sizeof (comment) - 1);
      }
      else if (commentlen && last && !last->personal)
      {
	terminate_buffer (comment, commentlen);
	last->personal = safe_strdup (comment);
      }

#ifdef EXACT_ADDRESS
      if (last && !last->val)
	last->val = mutt_substrdup (begin, s);
#endif
      commentlen = 0;
      phraselen = 0;
      s++;
#ifdef EXACT_ADDRESS
      begin = skip_email_wsp(s);
#endif
    }
    else if (*s == '(')
    {
      if (commentlen && commentlen < sizeof (comment) - 1)
	comment[commentlen++] = ' ';
      if ((ps = next_token (s, comment, &commentlen, sizeof (comment) - 1)) == NULL)
      {
	rfc822_free_address (&top);
	return NULL;
      }
      s = ps;
    }
    else if (*s == '"')
    {
      if (phraselen && phraselen < sizeof (phrase) - 1)
        phrase[phraselen++] = ' ';
      if ((ps = parse_quote (s + 1, phrase, &phraselen, sizeof (phrase) - 1)) == NULL)
      {
        rfc822_free_address (&top);
        return NULL;
      }
      s = ps;
    }
    else if (*s == '[')
    {
      if (phraselen && phraselen < sizeof (phrase) - 1 && ws_pending)
	phrase[phraselen++] = ' ';
      if (phraselen < sizeof (phrase) - 1)
        phrase[phraselen++] = '[';
      if ((ps = parse_literal (s + 1, phrase, &phraselen, sizeof (phrase) - 1)) == NULL)
      {
        rfc822_free_address (&top);
        return NULL;
      }
      s = ps;
    }
    else if (*s == ':')
    {
      cur = rfc822_new_address ();
      terminate_buffer (phrase, phraselen);
      cur->mailbox = safe_strdup (phrase);
      cur->group = 1;

      if (last)
	last->next = cur;
      else
	top = cur;
      last = cur;

#ifdef EXACT_ADDRESS
      last->val = mutt_substrdup (begin, s);
#endif

      phraselen = 0;
      commentlen = 0;
      s++;
#ifdef EXACT_ADDRESS
      begin = skip_email_wsp(s);
#endif
    }
    else if (*s == ';')
    {
      if (phraselen)
      {
	terminate_buffer (phrase, phraselen);
	add_addrspec (&top, &last, phrase, comment, &commentlen, sizeof (comment) - 1);
      }
      else if (commentlen && last && !last->personal)
      {
	terminate_buffer (comment, commentlen);
	last->personal = safe_strdup (comment);
      }
#ifdef EXACT_ADDRESS
      if (last && !last->val)
	last->val = mutt_substrdup (begin, s);
#endif

      /* add group terminator */
      if (last)
      {
	last->next = rfc822_new_address ();
	last = last->next;
      }

      phraselen = 0;
      commentlen = 0;
#ifdef EXACT_ADDRESS
      begin = s;
#endif
      s++;
    }
    else if (*s == '<')
    {
      terminate_buffer (phrase, phraselen);
      cur = rfc822_new_address ();
      if (phraselen)
	cur->personal = safe_strdup (phrase);
      if ((ps = parse_route_addr (s + 1, comment, &commentlen, sizeof (comment) - 1, cur)) == NULL)
      {
	rfc822_free_address (&top);
	rfc822_free_address (&cur);
	return NULL;
      }

      if (last)
	last->next = cur;
      else
	top = cur;
      last = cur;

      phraselen = 0;
      commentlen = 0;
      s = ps;
    }
    else
    {
      if (phraselen && phraselen < sizeof (phrase) - 1 && ws_pending)
	phrase[phraselen++] = ' ';
      if ((ps = next_token (s, phrase, &phraselen, sizeof (phrase) - 1)) == NULL)
      {
	rfc822_free_address (&top);
	return NULL;
      }
      s = ps;
    }
    ws_pending = is_email_wsp(*s);
    s = skip_email_wsp(s);
  }

  if (phraselen)
  {
    terminate_buffer (phrase, phraselen);
    terminate_buffer (comment, commentlen);
    add_addrspec (&top, &last, phrase, comment, &commentlen, sizeof (comment) - 1);
  }
  else if (commentlen && last && !last->personal)
  {
    terminate_buffer (comment, commentlen);
    last->personal = safe_strdup (comment);
  }
#ifdef EXACT_ADDRESS
  if (last && !last->val)
    last->val = mutt_substrdup (begin, s - nl < begin ? begin : s - nl);
#endif

  return top;
}