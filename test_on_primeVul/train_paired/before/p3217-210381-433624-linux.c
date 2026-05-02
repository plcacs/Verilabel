int ip_check_mc_rcu(struct in_device *in_dev, __be32 mc_addr, __be32 src_addr, u8 proto)
{
	struct ip_mc_list *im;
	struct ip_mc_list __rcu **mc_hash;
	struct ip_sf_list *psf;
	int rv = 0;

	mc_hash = rcu_dereference(in_dev->mc_hash);
	if (mc_hash) {
		u32 hash = hash_32((__force u32)mc_addr, MC_HASH_SZ_LOG);

		for (im = rcu_dereference(mc_hash[hash]);
		     im != NULL;
		     im = rcu_dereference(im->next_hash)) {
			if (im->multiaddr == mc_addr)
				break;
		}
	} else {
		for_each_pmc_rcu(in_dev, im) {
			if (im->multiaddr == mc_addr)
				break;
		}
	}
	if (im && proto == IPPROTO_IGMP) {
		rv = 1;
	} else if (im) {
		if (src_addr) {
			for (psf = im->sources; psf; psf = psf->sf_next) {
				if (psf->sf_inaddr == src_addr)
					break;
			}
			if (psf)
				rv = psf->sf_count[MCAST_INCLUDE] ||
					psf->sf_count[MCAST_EXCLUDE] !=
					im->sfcount[MCAST_EXCLUDE];
			else
				rv = im->sfcount[MCAST_EXCLUDE] != 0;
		} else
			rv = 1; /* unspecified source; tentatively allow */
	}
	return rv;
}