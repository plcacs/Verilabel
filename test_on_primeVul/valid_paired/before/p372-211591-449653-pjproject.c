PJ_DEF(pj_status_t) pjmedia_sdp_neg_modify_local_offer2(
                                    pj_pool_t *pool,
				    pjmedia_sdp_neg *neg,
                                    unsigned flags,
				    const pjmedia_sdp_session *local)
{
    pjmedia_sdp_session *new_offer;
    pjmedia_sdp_session *old_offer;
    char media_used[PJMEDIA_MAX_SDP_MEDIA];
    unsigned oi; /* old offer media index */
    pj_status_t status;

    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(pool && neg && local, PJ_EINVAL);

    /* Can only do this in STATE_DONE. */
    PJ_ASSERT_RETURN(neg->state == PJMEDIA_SDP_NEG_STATE_DONE, 
		     PJMEDIA_SDPNEG_EINSTATE);

    /* Validate the new offer */
    status = pjmedia_sdp_validate(local);
    if (status != PJ_SUCCESS)
	return status;

    /* Change state to STATE_LOCAL_OFFER */
    neg->state = PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER;

    /* Init vars */
    pj_bzero(media_used, sizeof(media_used));
    old_offer = neg->active_local_sdp;
    new_offer = pjmedia_sdp_session_clone(pool, local);

    /* RFC 3264 Section 8: When issuing an offer that modifies the session,
     * the "o=" line of the new SDP MUST be identical to that in the
     * previous SDP, except that the version in the origin field MUST
     * increment by one from the previous SDP.
     */
    pj_strdup(pool, &new_offer->origin.user, &old_offer->origin.user);
    new_offer->origin.id = old_offer->origin.id;

    pj_strdup(pool, &new_offer->origin.net_type, &old_offer->origin.net_type);
    pj_strdup(pool, &new_offer->origin.addr_type,&old_offer->origin.addr_type);
    pj_strdup(pool, &new_offer->origin.addr, &old_offer->origin.addr);

    if ((flags & PJMEDIA_SDP_NEG_ALLOW_MEDIA_CHANGE) == 0) {
       /* Generating the new offer, in the case media lines doesn't match the
        * active SDP (e.g. current/active SDP's have m=audio and m=video lines,
        * and the new offer only has m=audio line), the negotiator will fix 
        * the new offer by reordering and adding the missing media line with 
        * port number set to zero.
        */
        for (oi = 0; oi < old_offer->media_count; ++oi) {
	    pjmedia_sdp_media *om;
	    pjmedia_sdp_media *nm;
	    unsigned ni; /* new offer media index */
	    pj_bool_t found = PJ_FALSE;

	    om = old_offer->media[oi];
	    for (ni = oi; ni < new_offer->media_count; ++ni) {
	        nm = new_offer->media[ni];
	        if (pj_strcmp(&nm->desc.media, &om->desc.media) == 0) {
		    if (ni != oi) {
		        /* The same media found but the position unmatched to
                         * the old offer, so let's put this media in the right
                         * place, and keep the order of the rest.
		         */
		        pj_array_insert(
                            new_offer->media,		 /* array    */
			    sizeof(new_offer->media[0]), /* elmt size*/
			    ni,				 /* count    */
		            oi,				 /* pos      */
			    &nm);			 /* new elmt */
		    }
		    found = PJ_TRUE;
		    break;
	        }
	    }
	    if (!found) {
	        pjmedia_sdp_media *m;

	        m = sdp_media_clone_deactivate(pool, om, om, local);

	        pj_array_insert(new_offer->media, sizeof(new_offer->media[0]),
			        new_offer->media_count++, oi, &m);
	    }
        }
    } else {
        /* If media type change is allowed, the negotiator only needs to fix 
         * the new offer by adding the missing media line(s) with port number
         * set to zero.
         */
        for (oi = new_offer->media_count; oi < old_offer->media_count; ++oi) {
            pjmedia_sdp_media *m;

	    m = sdp_media_clone_deactivate(pool, old_offer->media[oi],
                                           old_offer->media[oi], local);

	    pj_array_insert(new_offer->media, sizeof(new_offer->media[0]),
	                    new_offer->media_count++, oi, &m);

        }
    }

    /* New_offer fixed */
#if PJMEDIA_SDP_NEG_COMPARE_BEFORE_INC_VERSION
    new_offer->origin.version = old_offer->origin.version;

    if (pjmedia_sdp_session_cmp(new_offer, neg->initial_sdp, 0) != PJ_SUCCESS)
    {
	++new_offer->origin.version;
    }    
#else
    new_offer->origin.version = old_offer->origin.version + 1;
#endif
    
    neg->initial_sdp_tmp = neg->initial_sdp;
    neg->initial_sdp = new_offer;
    neg->neg_local_sdp = pjmedia_sdp_session_clone(pool, new_offer);

    return PJ_SUCCESS;
}