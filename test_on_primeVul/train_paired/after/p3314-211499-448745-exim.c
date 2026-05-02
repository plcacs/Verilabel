pam_converse (int num_msg, PAM_CONVERSE_ARG2_TYPE **msg,
  struct pam_response **resp, void *appdata_ptr)
{
int sep = 0;
struct pam_response *reply;

/* It seems that PAM frees reply[] */

if (  pam_arg_ended
   || !(reply = malloc(sizeof(struct pam_response) * num_msg)))
  return PAM_CONV_ERR;

for (int i = 0; i < num_msg; i++)
  {
  uschar *arg;
  switch (msg[i]->msg_style)
    {
    case PAM_PROMPT_ECHO_ON:
    case PAM_PROMPT_ECHO_OFF:
      if (!(arg = string_nextinlist(&pam_args, &sep, NULL, 0)))
	{
	arg = US"";
	pam_arg_ended = TRUE;
	}
      reply[i].resp = strdup(CCS arg); /* Use libc malloc, PAM frees resp directly*/
      reply[i].resp_retcode = PAM_SUCCESS;
      break;

    case PAM_TEXT_INFO:    /* Just acknowledge messages */
    case PAM_ERROR_MSG:
      reply[i].resp_retcode = PAM_SUCCESS;
      reply[i].resp = NULL;
      break;

    default:  /* Must be an error of some sort... */
      free(reply);
      pam_conv_had_error = TRUE;
      return PAM_CONV_ERR;
    }
  }

*resp = reply;
return PAM_SUCCESS;
}