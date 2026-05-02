sdap_ad_tokengroups_get_posix_members(TALLOC_CTX *mem_ctx,
                         struct sdap_ad_tokengroups_initgr_posix_state *state,
                         size_t num_sids,
                         char **sids,
                         size_t *_num_missing,
                         char ***_missing,
                         size_t *_num_valid,
                         char ***_valid_groups)
{
    TALLOC_CTX *tmp_ctx = NULL;
    struct sss_domain_info *domain = NULL;
    struct ldb_message *msg = NULL;
    const char *attrs[] = {SYSDB_NAME, NULL};
    const char *name = NULL;
    char *sid = NULL;
    char **valid_groups = NULL;
    size_t num_valid_groups;
    char **missing_sids = NULL;
    size_t num_missing_sids;
    size_t i;
    errno_t ret;

    tmp_ctx = talloc_new(NULL);
    if (tmp_ctx == NULL) {
        DEBUG(SSSDBG_CRIT_FAILURE, "talloc_new() failed\n");
        ret = ENOMEM;
        goto done;
    }

    num_valid_groups = 0;
    valid_groups = talloc_zero_array(tmp_ctx, char*, num_sids + 1);
    if (valid_groups == NULL) {
        ret = ENOMEM;
        goto done;
    }

    num_missing_sids = 0;
    missing_sids = talloc_zero_array(tmp_ctx, char*, num_sids + 1);
    if (missing_sids == NULL) {
        ret = ENOMEM;
        goto done;
    }

    /* For each SID check if it is already present in the cache. If yes, we
     * will get name of the group and update the membership. Otherwise we need
     * to remember the SID and download missing groups one by one. */
    for (i = 0; i < num_sids; i++) {
        sid = sids[i];
        DEBUG(SSSDBG_TRACE_LIBS, "Processing membership SID [%s]\n", sid);

        domain = sss_get_domain_by_sid_ldap_fallback(state->domain, sid);
        if (domain == NULL) {
            DEBUG(SSSDBG_MINOR_FAILURE, "Domain not found for SID %s\n", sid);
            continue;
        }

        ret = sysdb_search_group_by_sid_str(tmp_ctx, domain->sysdb, domain,
                                            sid, attrs, &msg);
        if (ret == EOK) {
            /* we will update membership of this group */
            name = ldb_msg_find_attr_as_string(msg, SYSDB_NAME, NULL);
            if (name == NULL) {
                DEBUG(SSSDBG_MINOR_FAILURE,
                      "Could not retrieve group name from sysdb\n");
                ret = EINVAL;
                goto done;
            }

            valid_groups[num_valid_groups] = sysdb_group_strdn(valid_groups,
                                                               domain->name,
                                                               name);
            if (valid_groups[num_valid_groups] == NULL) {
                ret = ENOMEM;
                goto done;
            }
            num_valid_groups++;
        } else if (ret == ENOENT) {
            if (_missing != NULL) {
                /* we need to download this group */
                missing_sids[num_missing_sids] = talloc_steal(missing_sids,
                                                              sid);
                num_missing_sids++;

                DEBUG(SSSDBG_TRACE_FUNC, "Missing SID %s will be downloaded\n",
                                          sid);
            }

            /* else: We have downloaded missing groups but some of them may
             * remained missing because they are outside of search base. We
             * will just ignore them and continue with the next group. */
        } else {
            DEBUG(SSSDBG_MINOR_FAILURE, "Could not look up SID %s in sysdb: "
                                         "[%s]\n", sid, strerror(ret));
            goto done;
        }
    }

    valid_groups[num_valid_groups] = NULL;
    missing_sids[num_missing_sids] = NULL;

    /* return list of missing groups */
    if (_missing != NULL) {
        *_missing = talloc_steal(mem_ctx, missing_sids);
        *_num_missing = num_missing_sids;
    }

    /* return list of missing groups */
    if (_valid_groups != NULL) {
        *_valid_groups = talloc_steal(mem_ctx, valid_groups);
        *_num_valid = num_valid_groups;
    }

    ret = EOK;

done:
    talloc_free(tmp_ctx);
    return ret;
}