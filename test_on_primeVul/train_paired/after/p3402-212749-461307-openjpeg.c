static OPJ_BOOL opj_t2_encode_packet(OPJ_UINT32 tileno,
                                     opj_tcd_tile_t * tile,
                                     opj_tcp_t * tcp,
                                     opj_pi_iterator_t *pi,
                                     OPJ_BYTE *dest,
                                     OPJ_UINT32 * p_data_written,
                                     OPJ_UINT32 length,
                                     opj_codestream_info_t *cstr_info,
                                     J2K_T2_MODE p_t2_mode,
                                     opj_event_mgr_t *p_manager)
{
    OPJ_UINT32 bandno, cblkno;
    OPJ_BYTE* c = dest;
    OPJ_UINT32 l_nb_bytes;
    OPJ_UINT32 compno = pi->compno;     /* component value */
    OPJ_UINT32 resno  = pi->resno;      /* resolution level value */
    OPJ_UINT32 precno = pi->precno;     /* precinct value */
    OPJ_UINT32 layno  = pi->layno;      /* quality layer value */
    OPJ_UINT32 l_nb_blocks;
    opj_tcd_band_t *band = 00;
    opj_tcd_cblk_enc_t* cblk = 00;
    opj_tcd_pass_t *pass = 00;

    opj_tcd_tilecomp_t *tilec = &tile->comps[compno];
    opj_tcd_resolution_t *res = &tilec->resolutions[resno];

    opj_bio_t *bio = 00;    /* BIO component */
    OPJ_BOOL packet_empty = OPJ_TRUE;

    /* <SOP 0xff91> */
    if (tcp->csty & J2K_CP_CSTY_SOP) {
        if (length < 6) {
            if (p_t2_mode == FINAL_PASS) {
                opj_event_msg(p_manager, EVT_ERROR,
                              "opj_t2_encode_packet(): only %u bytes remaining in "
                              "output buffer. %u needed.\n",
                              length, 6);
            }
            return OPJ_FALSE;
        }
        c[0] = 255;
        c[1] = 145;
        c[2] = 0;
        c[3] = 4;
#if 0
        c[4] = (tile->packno % 65536) / 256;
        c[5] = (tile->packno % 65536) % 256;
#else
        c[4] = (tile->packno >> 8) & 0xff; /* packno is uint32_t */
        c[5] = tile->packno & 0xff;
#endif
        c += 6;
        length -= 6;
    }
    /* </SOP> */

    if (!layno) {
        band = res->bands;

        for (bandno = 0; bandno < res->numbands; ++bandno, ++band) {
            opj_tcd_precinct_t *prc;

            /* Skip empty bands */
            if (opj_tcd_is_band_empty(band)) {
                continue;
            }

            prc = &band->precincts[precno];
            opj_tgt_reset(prc->incltree);
            opj_tgt_reset(prc->imsbtree);

            l_nb_blocks = prc->cw * prc->ch;
            for (cblkno = 0; cblkno < l_nb_blocks; ++cblkno) {
                cblk = &prc->cblks.enc[cblkno];

                cblk->numpasses = 0;
                opj_tgt_setvalue(prc->imsbtree, cblkno, band->numbps - (OPJ_INT32)cblk->numbps);
            }
        }
    }

    bio = opj_bio_create();
    if (!bio) {
        /* FIXME event manager error callback */
        return OPJ_FALSE;
    }
    opj_bio_init_enc(bio, c, length);

    /* Check if the packet is empty */
    /* Note: we could also skip that step and always write a packet header */
    band = res->bands;
    for (bandno = 0; bandno < res->numbands; ++bandno, ++band) {
        opj_tcd_precinct_t *prc;
        /* Skip empty bands */
        if (opj_tcd_is_band_empty(band)) {
            continue;
        }

        prc = &band->precincts[precno];
        l_nb_blocks = prc->cw * prc->ch;
        cblk = prc->cblks.enc;
        for (cblkno = 0; cblkno < l_nb_blocks; cblkno++, ++cblk) {
            opj_tcd_layer_t *layer = &cblk->layers[layno];

            /* if cblk not included, go to the next cblk  */
            if (!layer->numpasses) {
                continue;
            }
            packet_empty = OPJ_FALSE;
            break;
        }
        if (!packet_empty) {
            break;
        }
    }

    opj_bio_write(bio, packet_empty ? 0 : 1, 1);           /* Empty header bit */


    /* Writing Packet header */
    band = res->bands;
    for (bandno = 0; !packet_empty &&
            bandno < res->numbands; ++bandno, ++band)      {
        opj_tcd_precinct_t *prc;

        /* Skip empty bands */
        if (opj_tcd_is_band_empty(band)) {
            continue;
        }

        prc = &band->precincts[precno];
        l_nb_blocks = prc->cw * prc->ch;
        cblk = prc->cblks.enc;

        for (cblkno = 0; cblkno < l_nb_blocks; ++cblkno) {
            opj_tcd_layer_t *layer = &cblk->layers[layno];

            if (!cblk->numpasses && layer->numpasses) {
                opj_tgt_setvalue(prc->incltree, cblkno, (OPJ_INT32)layno);
            }

            ++cblk;
        }

        cblk = prc->cblks.enc;
        for (cblkno = 0; cblkno < l_nb_blocks; cblkno++) {
            opj_tcd_layer_t *layer = &cblk->layers[layno];
            OPJ_UINT32 increment = 0;
            OPJ_UINT32 nump = 0;
            OPJ_UINT32 len = 0, passno;
            OPJ_UINT32 l_nb_passes;

            /* cblk inclusion bits */
            if (!cblk->numpasses) {
                opj_tgt_encode(bio, prc->incltree, cblkno, (OPJ_INT32)(layno + 1));
            } else {
                opj_bio_write(bio, layer->numpasses != 0, 1);
            }

            /* if cblk not included, go to the next cblk  */
            if (!layer->numpasses) {
                ++cblk;
                continue;
            }

            /* if first instance of cblk --> zero bit-planes information */
            if (!cblk->numpasses) {
                cblk->numlenbits = 3;
                opj_tgt_encode(bio, prc->imsbtree, cblkno, 999);
            }

            /* number of coding passes included */
            opj_t2_putnumpasses(bio, layer->numpasses);
            l_nb_passes = cblk->numpasses + layer->numpasses;
            pass = cblk->passes +  cblk->numpasses;

            /* computation of the increase of the length indicator and insertion in the header     */
            for (passno = cblk->numpasses; passno < l_nb_passes; ++passno) {
                ++nump;
                len += pass->len;

                if (pass->term || passno == (cblk->numpasses + layer->numpasses) - 1) {
                    increment = (OPJ_UINT32)opj_int_max((OPJ_INT32)increment,
                                                        opj_int_floorlog2((OPJ_INT32)len) + 1
                                                        - ((OPJ_INT32)cblk->numlenbits + opj_int_floorlog2((OPJ_INT32)nump)));
                    len = 0;
                    nump = 0;
                }

                ++pass;
            }
            opj_t2_putcommacode(bio, (OPJ_INT32)increment);

            /* computation of the new Length indicator */
            cblk->numlenbits += increment;

            pass = cblk->passes +  cblk->numpasses;
            /* insertion of the codeword segment length */
            for (passno = cblk->numpasses; passno < l_nb_passes; ++passno) {
                nump++;
                len += pass->len;

                if (pass->term || passno == (cblk->numpasses + layer->numpasses) - 1) {
                    opj_bio_write(bio, (OPJ_UINT32)len,
                                  cblk->numlenbits + (OPJ_UINT32)opj_int_floorlog2((OPJ_INT32)nump));
                    len = 0;
                    nump = 0;
                }
                ++pass;
            }

            ++cblk;
        }
    }

    if (!opj_bio_flush(bio)) {
        opj_bio_destroy(bio);
        return OPJ_FALSE;               /* modified to eliminate longjmp !! */
    }

    l_nb_bytes = (OPJ_UINT32)opj_bio_numbytes(bio);
    c += l_nb_bytes;
    length -= l_nb_bytes;

    opj_bio_destroy(bio);

    /* <EPH 0xff92> */
    if (tcp->csty & J2K_CP_CSTY_EPH) {
        if (length < 2) {
            if (p_t2_mode == FINAL_PASS) {
                opj_event_msg(p_manager, EVT_ERROR,
                              "opj_t2_encode_packet(): only %u bytes remaining in "
                              "output buffer. %u needed.\n",
                              length, 2);
            }
            return OPJ_FALSE;
        }
        c[0] = 255;
        c[1] = 146;
        c += 2;
        length -= 2;
    }
    /* </EPH> */

    /* << INDEX */
    /* End of packet header position. Currently only represents the distance to start of packet
       Will be updated later by incrementing with packet start value*/
    if (cstr_info && cstr_info->index_write) {
        opj_packet_info_t *info_PK = &cstr_info->tile[tileno].packet[cstr_info->packno];
        info_PK->end_ph_pos = (OPJ_INT32)(c - dest);
    }
    /* INDEX >> */

    /* Writing the packet body */
    band = res->bands;
    for (bandno = 0; !packet_empty && bandno < res->numbands; bandno++, ++band) {
        opj_tcd_precinct_t *prc;

        /* Skip empty bands */
        if (opj_tcd_is_band_empty(band)) {
            continue;
        }

        prc = &band->precincts[precno];
        l_nb_blocks = prc->cw * prc->ch;
        cblk = prc->cblks.enc;

        for (cblkno = 0; cblkno < l_nb_blocks; ++cblkno) {
            opj_tcd_layer_t *layer = &cblk->layers[layno];

            if (!layer->numpasses) {
                ++cblk;
                continue;
            }

            if (layer->len > length) {
                if (p_t2_mode == FINAL_PASS) {
                    opj_event_msg(p_manager, EVT_ERROR,
                                  "opj_t2_encode_packet(): only %u bytes remaining in "
                                  "output buffer. %u needed.\n",
                                  length, layer->len);
                }
                return OPJ_FALSE;
            }

            memcpy(c, layer->data, layer->len);
            cblk->numpasses += layer->numpasses;
            c += layer->len;
            length -= layer->len;

            /* << INDEX */
            if (cstr_info && cstr_info->index_write) {
                opj_packet_info_t *info_PK = &cstr_info->tile[tileno].packet[cstr_info->packno];
                info_PK->disto += layer->disto;
                if (cstr_info->D_max < info_PK->disto) {
                    cstr_info->D_max = info_PK->disto;
                }
            }

            ++cblk;
            /* INDEX >> */
        }
    }

    assert(c >= dest);
    * p_data_written += (OPJ_UINT32)(c - dest);

    return OPJ_TRUE;
}