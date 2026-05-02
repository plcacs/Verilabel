static struct tevent_req *sdap_initgr_rfc2307bis_send(
        TALLOC_CTX *memctx,
        struct tevent_context *ev,
        struct sdap_options *opts,
        struct sdap_domain *sdom,
        struct sdap_handle *sh,
        const char *name,
        const char *orig_dn)
{
    errno_t ret;
    struct tevent_req *req;
    struct sdap_initgr_rfc2307bis_state *state;
    const char **attr_filter;
    char *clean_orig_dn;
    bool use_id_mapping;

    req = tevent_req_create(memctx, &state, struct sdap_initgr_rfc2307bis_state);
    if (!req) return NULL;

    state->ev = ev;
    state->opts = opts;
    state->sysdb = sdom->dom->sysdb;
    state->dom = sdom->dom;
    state->sh = sh;
    state->op = NULL;
    state->name = name;
    state->direct_groups = NULL;
    state->num_direct_parents = 0;
    state->timeout = dp_opt_get_int(state->opts->basic, SDAP_SEARCH_TIMEOUT);
    state->base_iter = 0;
    state->search_bases = sdom->group_search_bases;
    state->orig_dn = orig_dn;

    if (!state->search_bases) {
        DEBUG(SSSDBG_CRIT_FAILURE,
              "Initgroups lookup request without a group search base\n");
        ret = EINVAL;
        goto done;
    }

    ret = sss_hash_create(state, 32, &state->group_hash);
    if (ret != EOK) {
        talloc_free(req);
        return NULL;
    }

    attr_filter = talloc_array(state, const char *, 2);
    if (!attr_filter) {
        ret = ENOMEM;
        goto done;
    }

    attr_filter[0] = opts->group_map[SDAP_AT_GROUP_MEMBER].name;
    attr_filter[1] = NULL;

    ret = build_attrs_from_map(state, opts->group_map, SDAP_OPTS_GROUP,
                               attr_filter, &state->attrs, NULL);
    if (ret != EOK) goto done;

    ret = sss_filter_sanitize(state, orig_dn, &clean_orig_dn);
    if (ret != EOK) goto done;

    use_id_mapping = sdap_idmap_domain_has_algorithmic_mapping(
                                                        opts->idmap_ctx,
                                                        sdom->dom->name,
                                                        sdom->dom->domain_id);

    state->base_filter =
            talloc_asprintf(state, "(&(%s=%s)(objectclass=%s)(%s=*)",
                            opts->group_map[SDAP_AT_GROUP_MEMBER].name,
                            clean_orig_dn,
                            opts->group_map[SDAP_OC_GROUP].name,
                            opts->group_map[SDAP_AT_GROUP_NAME].name);
    if (!state->base_filter) {
        ret = ENOMEM;
        goto done;
    }

    if (use_id_mapping) {
        /* When mapping IDs or looking for SIDs, we don't want to limit
         * ourselves to groups with a GID value. But there must be a SID to map
         * from.
         */
        state->base_filter = talloc_asprintf_append(state->base_filter,
                                        "(%s=*))",
                                        opts->group_map[SDAP_AT_GROUP_OBJECTSID].name);
    } else {
        /* When not ID-mapping, make sure there is a non-NULL UID */
        state->base_filter = talloc_asprintf_append(state->base_filter,
                                        "(&(%s=*)(!(%s=0))))",
                                        opts->group_map[SDAP_AT_GROUP_GID].name,
                                        opts->group_map[SDAP_AT_GROUP_GID].name);
    }
    if (!state->base_filter) {
        talloc_zfree(req);
        return NULL;
    }


    talloc_zfree(clean_orig_dn);

    ret = sdap_initgr_rfc2307bis_next_base(req);

done:
    if (ret != EOK) {
        tevent_req_error(req, ret);
        tevent_req_post(req, ev);
    }
    return req;
}