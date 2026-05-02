policy_summarize(smartlist_t *policy)
{
  smartlist_t *summary = policy_summary_create();
  smartlist_t *accepts, *rejects;
  int i, last, start_prt;
  size_t accepts_len, rejects_len, shorter_len, final_size;
  char *accepts_str = NULL, *rejects_str = NULL, *shorter_str, *result;
  const char *prefix;

  tor_assert(policy);

  /* Create the summary list */
  SMARTLIST_FOREACH(policy, addr_policy_t *, p, {
    policy_summary_add_item(summary, p);
  });

  /* Now create two lists of strings, one for accepted and one
   * for rejected ports.  We take care to merge ranges so that
   * we avoid getting stuff like "1-4,5-9,10", instead we want
   * "1-10"
   */
  i = 0;
  start_prt = 1;
  accepts = smartlist_create();
  rejects = smartlist_create();
  while (1) {
    last = i == smartlist_len(summary)-1;
    if (last ||
        AT(i)->accepted != AT(i+1)->accepted) {
      char buf[POLICY_BUF_LEN];

      if (start_prt == AT(i)->prt_max)
        tor_snprintf(buf, sizeof(buf), "%d", start_prt);
      else
        tor_snprintf(buf, sizeof(buf), "%d-%d", start_prt, AT(i)->prt_max);

      if (AT(i)->accepted)
        smartlist_add(accepts, tor_strdup(buf));
      else
        smartlist_add(rejects, tor_strdup(buf));

      if (last)
        break;

      start_prt = AT(i+1)->prt_min;
    };
    i++;
  };

  /* Figure out which of the two stringlists will be shorter and use
   * that to build the result
   */
  if (smartlist_len(accepts) == 0) { /* no exits at all */
    result = tor_strdup("reject 1-65535");
    goto cleanup;
  }
  if (smartlist_len(rejects) == 0) { /* no rejects at all */
    result = tor_strdup("accept 1-65535");
    goto cleanup;
  }

  accepts_str = smartlist_join_strings(accepts, ",", 0, &accepts_len);
  rejects_str = smartlist_join_strings(rejects, ",", 0, &rejects_len);

  if (rejects_len > MAX_EXITPOLICY_SUMMARY_LEN &&
      accepts_len > MAX_EXITPOLICY_SUMMARY_LEN) {
    char *c;
    shorter_str = accepts_str;
    prefix = "accept";

    c = shorter_str + (MAX_EXITPOLICY_SUMMARY_LEN-strlen(prefix)-1);
    while (*c != ',' && c >= shorter_str)
      c--;
    tor_assert(c >= shorter_str);
    tor_assert(*c == ',');
    *c = '\0';

    shorter_len = strlen(shorter_str);
  } else if (rejects_len < accepts_len) {
    shorter_str = rejects_str;
    shorter_len = rejects_len;
    prefix = "reject";
  } else {
    shorter_str = accepts_str;
    shorter_len = accepts_len;
    prefix = "accept";
  }

  final_size = strlen(prefix)+1+shorter_len+1;
  tor_assert(final_size <= MAX_EXITPOLICY_SUMMARY_LEN+1);
  result = tor_malloc(final_size);
  tor_snprintf(result, final_size, "%s %s", prefix, shorter_str);

cleanup:
  /* cleanup */
  SMARTLIST_FOREACH(summary, policy_summary_item_t *, s, tor_free(s));
  smartlist_free(summary);

  tor_free(accepts_str);
  SMARTLIST_FOREACH(accepts, char *, s, tor_free(s));
  smartlist_free(accepts);

  tor_free(rejects_str);
  SMARTLIST_FOREACH(rejects, char *, s, tor_free(s));
  smartlist_free(rejects);

  return result;
}