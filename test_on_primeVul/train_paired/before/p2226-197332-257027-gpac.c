static u8 BS_ReadByte(GF_BitStream *bs)
{
	Bool is_eos;
	if (bs->bsmode == GF_BITSTREAM_READ) {
		u8 res;
		if (bs->position >= bs->size) {
			if (bs->EndOfStream) bs->EndOfStream(bs->par);
			if (!bs->overflow_state) bs->overflow_state = 1;
			return 0;
		}
		res = bs->original[bs->position++];

		if (bs->remove_emul_prevention_byte) {
			if ((bs->nb_zeros==2) && (res==0x03) && (bs->position<bs->size) && (bs->original[bs->position]<0x04)) {
				bs->nb_zeros = 0;
				res = bs->original[bs->position++];
			}
			if (!res) bs->nb_zeros++;
			else bs->nb_zeros = 0;
		}
		return res;
	}
	if (bs->cache_write)
		bs_flush_write_cache(bs);

	is_eos = gf_feof(bs->stream);

	/*we are in FILE mode, test for end of file*/
	if (!is_eos || bs->cache_read) {
		u8 res;
		Bool loc_eos=GF_FALSE;
		assert(bs->position<=bs->size);
		bs->position++;

		res = gf_bs_load_byte(bs, &loc_eos);
		if (loc_eos) goto bs_eof;

		if (bs->remove_emul_prevention_byte) {
			if ((bs->nb_zeros==2) && (res==0x03) && (bs->position<bs->size)) {
				u8 next = gf_bs_load_byte(bs, &loc_eos);
				if (next < 0x04) {
					bs->nb_zeros = 0;
					res = next;
					bs->position++;
				} else {
					gf_bs_seek(bs, bs->position);
				}
			}
			if (!res) bs->nb_zeros++;
			else bs->nb_zeros = 0;
		}
		return res;
	}

bs_eof:
	if (bs->EndOfStream) {
		bs->EndOfStream(bs->par);
		if (!bs->overflow_state) bs->overflow_state = 1;
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[BS] Attempt to overread bitstream\n"));
	}
	assert(bs->position <= 1+bs->size);
	return 0;
}