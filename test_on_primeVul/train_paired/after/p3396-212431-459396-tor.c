set_routerstatus_from_routerinfo(routerstatus_t *rs,
                                 routerinfo_t *ri, time_t now,
                                 int naming, int listbadexits,
                                 int listbaddirs, int vote_on_hsdirs)
{
  const or_options_t *options = get_options();
  int unstable_version =
    !tor_version_as_new_as(ri->platform,"0.1.1.16-rc-cvs");
  memset(rs, 0, sizeof(routerstatus_t));

  rs->is_authority =
    router_digest_is_trusted_dir(ri->cache_info.identity_digest);

  /* Already set by compute_performance_thresholds. */
  rs->is_exit = ri->is_exit;
  rs->is_stable = ri->is_stable =
    router_is_active(ri, now) &&
    !dirserv_thinks_router_is_unreliable(now, ri, 1, 0) &&
    !unstable_version;
  rs->is_fast = ri->is_fast =
    router_is_active(ri, now) &&
    !dirserv_thinks_router_is_unreliable(now, ri, 0, 1);
  rs->is_running = ri->is_running; /* computed above */

  if (naming) {
    uint32_t name_status = dirserv_get_name_status(
                         ri->cache_info.identity_digest, ri->nickname);
    rs->is_named = (naming && (name_status & FP_NAMED)) ? 1 : 0;
    rs->is_unnamed = (naming && (name_status & FP_UNNAMED)) ? 1 : 0;
  }
  rs->is_valid = ri->is_valid;

  if (rs->is_fast &&
      (router_get_advertised_bandwidth(ri) >= BANDWIDTH_TO_GUARANTEE_GUARD ||
       router_get_advertised_bandwidth(ri) >=
                              MIN(guard_bandwidth_including_exits,
                                  guard_bandwidth_excluding_exits)) &&
      (options->GiveGuardFlagTo_CVE_2011_2768_VulnerableRelays ||
       is_router_version_good_for_possible_guard(ri->platform))) {
    long tk = rep_hist_get_weighted_time_known(
                                      ri->cache_info.identity_digest, now);
    double wfu = rep_hist_get_weighted_fractional_uptime(
                                      ri->cache_info.identity_digest, now);
    rs->is_possible_guard = (wfu >= guard_wfu && tk >= guard_tk) ? 1 : 0;
  } else {
    rs->is_possible_guard = 0;
  }
  rs->is_bad_directory = listbaddirs && ri->is_bad_directory;
  rs->is_bad_exit = listbadexits && ri->is_bad_exit;
  ri->is_hs_dir = dirserv_thinks_router_is_hs_dir(ri, now);
  rs->is_hs_dir = vote_on_hsdirs && ri->is_hs_dir;
  rs->is_v2_dir = ri->dir_port != 0;

  if (!strcasecmp(ri->nickname, UNNAMED_ROUTER_NICKNAME))
    rs->is_named = rs->is_unnamed = 0;

  rs->published_on = ri->cache_info.published_on;
  memcpy(rs->identity_digest, ri->cache_info.identity_digest, DIGEST_LEN);
  memcpy(rs->descriptor_digest, ri->cache_info.signed_descriptor_digest,
         DIGEST_LEN);
  rs->addr = ri->addr;
  strlcpy(rs->nickname, ri->nickname, sizeof(rs->nickname));
  rs->or_port = ri->or_port;
  rs->dir_port = ri->dir_port;
}