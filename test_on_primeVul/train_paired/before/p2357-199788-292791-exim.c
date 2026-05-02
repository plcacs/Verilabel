auth_spa_server(auth_instance *ablock, uschar *data)
{
auth_spa_options_block *ob = (auth_spa_options_block *)(ablock->options_block);
uint8x lmRespData[24];
uint8x ntRespData[24];
SPAAuthRequest request;
SPAAuthChallenge challenge;
SPAAuthResponse  response;
SPAAuthResponse  *responseptr = &response;
uschar msgbuf[2048];
uschar *clearpass, *s;

/* send a 334, MS Exchange style, and grab the client's request,
unless we already have it via an initial response. */

if (!*data && auth_get_no64_data(&data, US"NTLM supported") != OK)
  return FAIL;

if (spa_base64_to_bits(CS &request, sizeof(request), CCS data) < 0)
  {
  DEBUG(D_auth) debug_printf("auth_spa_server(): bad base64 data in "
    "request: %s\n", data);
  return FAIL;
  }

/* create a challenge and send it back */

spa_build_auth_challenge(&request, &challenge);
spa_bits_to_base64(msgbuf, US &challenge, spa_request_length(&challenge));

if (auth_get_no64_data(&data, msgbuf) != OK)
  return FAIL;

/* dump client response */
if (spa_base64_to_bits(CS &response, sizeof(response), CCS data) < 0)
  {
  DEBUG(D_auth) debug_printf("auth_spa_server(): bad base64 data in "
    "response: %s\n", data);
  return FAIL;
  }

/***************************************************************
PH 07-Aug-2003: The original code here was this:

Ustrcpy(msgbuf, unicodeToString(((char*)responseptr) +
  IVAL(&responseptr->uUser.offset,0),
  SVAL(&responseptr->uUser.len,0)/2) );

However, if the response data is too long, unicodeToString bombs out on
an assertion failure. It uses a 1024 fixed buffer. Bombing out is not a good
idea. It's too messy to try to rework that function to return an error because
it is called from a number of other places in the auth-spa.c module. Instead,
since it is a very small function, I reproduce its code here, with a size check
that causes failure if the size of msgbuf is exceeded. ****/

  {
  int i;
  char * p = (CS responseptr) + IVAL(&responseptr->uUser.offset,0);
  int len = SVAL(&responseptr->uUser.len,0)/2;

  if (p + len*2 >= CS (responseptr+1))
    {
    DEBUG(D_auth)
      debug_printf("auth_spa_server(): bad uUser spec in response\n");
    return FAIL;
    }

  if (len + 1 >= sizeof(msgbuf)) return FAIL;
  for (i = 0; i < len; ++i)
    {
    msgbuf[i] = *p & 0x7f;
    p += 2;
    }
  msgbuf[i] = 0;
  }

/***************************************************************/

/* Put the username in $auth1 and $1. The former is now the preferred variable;
the latter is the original variable. These have to be out of stack memory, and
need to be available once known even if not authenticated, for error messages
(server_set_id, which only makes it to authenticated_id if we return OK) */

auth_vars[0] = expand_nstring[1] = string_copy(msgbuf);
expand_nlength[1] = Ustrlen(msgbuf);
expand_nmax = 1;

debug_print_string(ablock->server_debug_string);    /* customized debug */

/* look up password */

if (!(clearpass = expand_string(ob->spa_serverpassword)))
  if (f.expand_string_forcedfail)
    {
    DEBUG(D_auth) debug_printf("auth_spa_server(): forced failure while "
      "expanding spa_serverpassword\n");
    return FAIL;
    }
  else
    {
    DEBUG(D_auth) debug_printf("auth_spa_server(): error while expanding "
      "spa_serverpassword: %s\n", expand_string_message);
    return DEFER;
    }

/* create local hash copy */

spa_smb_encrypt(clearpass, challenge.challengeData, lmRespData);
spa_smb_nt_encrypt(clearpass, challenge.challengeData, ntRespData);

/* compare NT hash (LM may not be available) */

s = (US responseptr) + IVAL(&responseptr->ntResponse.offset,0);
if (s + 24 >= US (responseptr+1))
  {
  DEBUG(D_auth)
    debug_printf("auth_spa_server(): bad ntRespData spec in response\n");
  return FAIL;
  }

if (memcmp(ntRespData, s, 24) == 0)
  return auth_check_serv_cond(ablock);	/* success. we have a winner. */

  /* Expand server_condition as an authorization check (PH) */

return FAIL;
}