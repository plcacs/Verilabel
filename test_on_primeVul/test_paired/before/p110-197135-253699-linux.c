ccp_run_aes_gcm_cmd(struct ccp_cmd_queue *cmd_q, struct ccp_cmd *cmd)
{
	struct ccp_aes_engine *aes = &cmd->u.aes;
	struct ccp_dm_workarea key, ctx, final_wa, tag;
	struct ccp_data src, dst;
	struct ccp_data aad;
	struct ccp_op op;
	unsigned int dm_offset;
	unsigned int authsize;
	unsigned int jobid;
	unsigned int ilen;
	bool in_place = true; /* Default value */
	__be64 *final;
	int ret;

	struct scatterlist *p_inp, sg_inp[2];
	struct scatterlist *p_tag, sg_tag[2];
	struct scatterlist *p_outp, sg_outp[2];
	struct scatterlist *p_aad;

	if (!aes->iv)
		return -EINVAL;

	if (!((aes->key_len == AES_KEYSIZE_128) ||
		(aes->key_len == AES_KEYSIZE_192) ||
		(aes->key_len == AES_KEYSIZE_256)))
		return -EINVAL;

	if (!aes->key) /* Gotta have a key SGL */
		return -EINVAL;

	/* Zero defaults to 16 bytes, the maximum size */
	authsize = aes->authsize ? aes->authsize : AES_BLOCK_SIZE;
	switch (authsize) {
	case 16:
	case 15:
	case 14:
	case 13:
	case 12:
	case 8:
	case 4:
		break;
	default:
		return -EINVAL;
	}

	/* First, decompose the source buffer into AAD & PT,
	 * and the destination buffer into AAD, CT & tag, or
	 * the input into CT & tag.
	 * It is expected that the input and output SGs will
	 * be valid, even if the AAD and input lengths are 0.
	 */
	p_aad = aes->src;
	p_inp = scatterwalk_ffwd(sg_inp, aes->src, aes->aad_len);
	p_outp = scatterwalk_ffwd(sg_outp, aes->dst, aes->aad_len);
	if (aes->action == CCP_AES_ACTION_ENCRYPT) {
		ilen = aes->src_len;
		p_tag = scatterwalk_ffwd(sg_tag, p_outp, ilen);
	} else {
		/* Input length for decryption includes tag */
		ilen = aes->src_len - authsize;
		p_tag = scatterwalk_ffwd(sg_tag, p_inp, ilen);
	}

	jobid = CCP_NEW_JOBID(cmd_q->ccp);

	memset(&op, 0, sizeof(op));
	op.cmd_q = cmd_q;
	op.jobid = jobid;
	op.sb_key = cmd_q->sb_key; /* Pre-allocated */
	op.sb_ctx = cmd_q->sb_ctx; /* Pre-allocated */
	op.init = 1;
	op.u.aes.type = aes->type;

	/* Copy the key to the LSB */
	ret = ccp_init_dm_workarea(&key, cmd_q,
				   CCP_AES_CTX_SB_COUNT * CCP_SB_BYTES,
				   DMA_TO_DEVICE);
	if (ret)
		return ret;

	dm_offset = CCP_SB_BYTES - aes->key_len;
	ret = ccp_set_dm_area(&key, dm_offset, aes->key, 0, aes->key_len);
	if (ret)
		goto e_key;
	ret = ccp_copy_to_sb(cmd_q, &key, op.jobid, op.sb_key,
			     CCP_PASSTHRU_BYTESWAP_256BIT);
	if (ret) {
		cmd->engine_error = cmd_q->cmd_error;
		goto e_key;
	}

	/* Copy the context (IV) to the LSB.
	 * There is an assumption here that the IV is 96 bits in length, plus
	 * a nonce of 32 bits. If no IV is present, use a zeroed buffer.
	 */
	ret = ccp_init_dm_workarea(&ctx, cmd_q,
				   CCP_AES_CTX_SB_COUNT * CCP_SB_BYTES,
				   DMA_BIDIRECTIONAL);
	if (ret)
		goto e_key;

	dm_offset = CCP_AES_CTX_SB_COUNT * CCP_SB_BYTES - aes->iv_len;
	ret = ccp_set_dm_area(&ctx, dm_offset, aes->iv, 0, aes->iv_len);
	if (ret)
		goto e_ctx;

	ret = ccp_copy_to_sb(cmd_q, &ctx, op.jobid, op.sb_ctx,
			     CCP_PASSTHRU_BYTESWAP_256BIT);
	if (ret) {
		cmd->engine_error = cmd_q->cmd_error;
		goto e_ctx;
	}

	op.init = 1;
	if (aes->aad_len > 0) {
		/* Step 1: Run a GHASH over the Additional Authenticated Data */
		ret = ccp_init_data(&aad, cmd_q, p_aad, aes->aad_len,
				    AES_BLOCK_SIZE,
				    DMA_TO_DEVICE);
		if (ret)
			goto e_ctx;

		op.u.aes.mode = CCP_AES_MODE_GHASH;
		op.u.aes.action = CCP_AES_GHASHAAD;

		while (aad.sg_wa.bytes_left) {
			ccp_prepare_data(&aad, NULL, &op, AES_BLOCK_SIZE, true);

			ret = cmd_q->ccp->vdata->perform->aes(&op);
			if (ret) {
				cmd->engine_error = cmd_q->cmd_error;
				goto e_aad;
			}

			ccp_process_data(&aad, NULL, &op);
			op.init = 0;
		}
	}

	op.u.aes.mode = CCP_AES_MODE_GCTR;
	op.u.aes.action = aes->action;

	if (ilen > 0) {
		/* Step 2: Run a GCTR over the plaintext */
		in_place = (sg_virt(p_inp) == sg_virt(p_outp)) ? true : false;

		ret = ccp_init_data(&src, cmd_q, p_inp, ilen,
				    AES_BLOCK_SIZE,
				    in_place ? DMA_BIDIRECTIONAL
					     : DMA_TO_DEVICE);
		if (ret)
			goto e_ctx;

		if (in_place) {
			dst = src;
		} else {
			ret = ccp_init_data(&dst, cmd_q, p_outp, ilen,
					    AES_BLOCK_SIZE, DMA_FROM_DEVICE);
			if (ret)
				goto e_src;
		}

		op.soc = 0;
		op.eom = 0;
		op.init = 1;
		while (src.sg_wa.bytes_left) {
			ccp_prepare_data(&src, &dst, &op, AES_BLOCK_SIZE, true);
			if (!src.sg_wa.bytes_left) {
				unsigned int nbytes = ilen % AES_BLOCK_SIZE;

				if (nbytes) {
					op.eom = 1;
					op.u.aes.size = (nbytes * 8) - 1;
				}
			}

			ret = cmd_q->ccp->vdata->perform->aes(&op);
			if (ret) {
				cmd->engine_error = cmd_q->cmd_error;
				goto e_dst;
			}

			ccp_process_data(&src, &dst, &op);
			op.init = 0;
		}
	}

	/* Step 3: Update the IV portion of the context with the original IV */
	ret = ccp_copy_from_sb(cmd_q, &ctx, op.jobid, op.sb_ctx,
			       CCP_PASSTHRU_BYTESWAP_256BIT);
	if (ret) {
		cmd->engine_error = cmd_q->cmd_error;
		goto e_dst;
	}

	ret = ccp_set_dm_area(&ctx, dm_offset, aes->iv, 0, aes->iv_len);
	if (ret)
		goto e_dst;

	ret = ccp_copy_to_sb(cmd_q, &ctx, op.jobid, op.sb_ctx,
			     CCP_PASSTHRU_BYTESWAP_256BIT);
	if (ret) {
		cmd->engine_error = cmd_q->cmd_error;
		goto e_dst;
	}

	/* Step 4: Concatenate the lengths of the AAD and source, and
	 * hash that 16 byte buffer.
	 */
	ret = ccp_init_dm_workarea(&final_wa, cmd_q, AES_BLOCK_SIZE,
				   DMA_BIDIRECTIONAL);
	if (ret)
		goto e_dst;
	final = (__be64 *)final_wa.address;
	final[0] = cpu_to_be64(aes->aad_len * 8);
	final[1] = cpu_to_be64(ilen * 8);

	memset(&op, 0, sizeof(op));
	op.cmd_q = cmd_q;
	op.jobid = jobid;
	op.sb_key = cmd_q->sb_key; /* Pre-allocated */
	op.sb_ctx = cmd_q->sb_ctx; /* Pre-allocated */
	op.init = 1;
	op.u.aes.type = aes->type;
	op.u.aes.mode = CCP_AES_MODE_GHASH;
	op.u.aes.action = CCP_AES_GHASHFINAL;
	op.src.type = CCP_MEMTYPE_SYSTEM;
	op.src.u.dma.address = final_wa.dma.address;
	op.src.u.dma.length = AES_BLOCK_SIZE;
	op.dst.type = CCP_MEMTYPE_SYSTEM;
	op.dst.u.dma.address = final_wa.dma.address;
	op.dst.u.dma.length = AES_BLOCK_SIZE;
	op.eom = 1;
	op.u.aes.size = 0;
	ret = cmd_q->ccp->vdata->perform->aes(&op);
	if (ret)
		goto e_dst;

	if (aes->action == CCP_AES_ACTION_ENCRYPT) {
		/* Put the ciphered tag after the ciphertext. */
		ccp_get_dm_area(&final_wa, 0, p_tag, 0, authsize);
	} else {
		/* Does this ciphered tag match the input? */
		ret = ccp_init_dm_workarea(&tag, cmd_q, authsize,
					   DMA_BIDIRECTIONAL);
		if (ret)
			goto e_tag;
		ret = ccp_set_dm_area(&tag, 0, p_tag, 0, authsize);
		if (ret)
			goto e_tag;

		ret = crypto_memneq(tag.address, final_wa.address,
				    authsize) ? -EBADMSG : 0;
		ccp_dm_free(&tag);
	}

e_tag:
	ccp_dm_free(&final_wa);

e_dst:
	if (ilen > 0 && !in_place)
		ccp_free_data(&dst, cmd_q);

e_src:
	if (ilen > 0)
		ccp_free_data(&src, cmd_q);

e_aad:
	if (aes->aad_len)
		ccp_free_data(&aad, cmd_q);

e_ctx:
	ccp_dm_free(&ctx);

e_key:
	ccp_dm_free(&key);

	return ret;
}