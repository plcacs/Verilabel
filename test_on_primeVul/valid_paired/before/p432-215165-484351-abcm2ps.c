static void get_over(struct SYMBOL *s)
{
	struct VOICE_S *p_voice, *p_voice2, *p_voice3;
	int range, voice, voice2, voice3;
static char tx_wrong_dur[] = "Wrong duration in voice overlay";
static char txt_no_note[] = "No note in voice overlay";

	/* treat the end of overlay */
	p_voice = curvoice;
	if (p_voice->ignore)
		return;
	if (s->abc_type == ABC_T_BAR
	 || s->u.v_over.type == V_OVER_E)  {
		if (!p_voice->last_sym) {
			error(1, s, txt_no_note);
			return;
		}
		p_voice->last_sym->sflags |= S_BEAM_END;
		over_bar = 0;
		if (over_time < 0) {
			error(1, s, "Erroneous end of voice overlap");
			return;
		}
		if (p_voice->time != over_mxtime)
			error(1, s, tx_wrong_dur);
		curvoice = &voice_tb[over_voice];
		over_mxtime = 0;
		over_voice = -1;
		over_time = -1;
		return;
	}

	/* treat the full overlay start */
	if (s->u.v_over.type == V_OVER_S) {
		over_voice = p_voice - voice_tb;
		over_time = p_voice->time;
		return;
	}

	/* (here is treated a new overlay - '&') */
	/* create the extra voice if not done yet */
	if (!p_voice->last_sym) {
		error(1, s, txt_no_note);
		return;
	}
	p_voice->last_sym->sflags |= S_BEAM_END;
	voice2 = s->u.v_over.voice;
	p_voice2 = &voice_tb[voice2];
	if (parsys->voice[voice2].range < 0) {
		int clone;

		if (cfmt.abc2pscompat) {
			error(1, s, "Cannot have %%%%abc2pscompat");
			cfmt.abc2pscompat = 0;
		}
		clone = p_voice->clone >= 0;
		p_voice2->id[0] = '&';
		p_voice2->id[1] = '\0';
		p_voice2->second = 1;
		parsys->voice[voice2].second = 1;
		p_voice2->scale = p_voice->scale;
		p_voice2->octave = p_voice->octave;
		p_voice2->transpose = p_voice->transpose;
		memcpy(&p_voice2->key, &p_voice->key,
					sizeof p_voice2->key);
		memcpy(&p_voice2->ckey, &p_voice->ckey,
					sizeof p_voice2->ckey);
		memcpy(&p_voice2->okey, &p_voice->okey,
					sizeof p_voice2->okey);
		p_voice2->posit = p_voice->posit;
		p_voice2->staff = p_voice->staff;
		p_voice2->cstaff = p_voice->cstaff;
		p_voice2->color = p_voice->color;
		p_voice2->map_name = p_voice->map_name;
		range = parsys->voice[p_voice - voice_tb].range;
		for (voice = 0; voice < MAXVOICE; voice++) {
			if (parsys->voice[voice].range > range)
				parsys->voice[voice].range += clone + 1;
		}
		parsys->voice[voice2].range = range + 1;
		voice_link(p_voice2);
		if (clone) {
			for (voice3 = MAXVOICE; --voice3 >= 0; ) {
				if (parsys->voice[voice3].range < 0)
					break;
			}
			if (voice3 > 0) {
				p_voice3 = &voice_tb[voice3];
				strcpy(p_voice3->id, p_voice2->id);
				p_voice3->second = 1;
				parsys->voice[voice3].second = 1;
				p_voice3->scale = voice_tb[p_voice->clone].scale;
				parsys->voice[voice3].range = range + 2;
				voice_link(p_voice3);
				p_voice2->clone = voice3;
			} else {
				error(1, s,
				      "Too many voices for overlay cloning");
			}
		}
	}
	voice = p_voice - voice_tb;
//	p_voice2->cstaff = p_voice2->staff = parsys->voice[voice2].staff
//			= parsys->voice[voice].staff;
//	if ((voice3 = p_voice2->clone) >= 0) {
//		p_voice3 = &voice_tb[voice3];
//		p_voice3->cstaff = p_voice3->staff
//				= parsys->voice[voice3].staff
//				= parsys->voice[p_voice->clone].staff;
//	}

	if (over_time < 0) {			/* first '&' in a measure */
		int time;

		over_bar = 1;
		over_mxtime = p_voice->time;
		over_voice = voice;
		time = p_voice2->time;
		for (s = p_voice->last_sym; /*s*/; s = s->prev) {
			if (s->type == BAR
			 || s->time <= time)	/* (if start of tune) */
				break;
		}
		over_time = s->time;
	} else {
		if (over_mxtime == 0)
			over_mxtime = p_voice->time;
		else if (p_voice->time != over_mxtime)
			error(1, s, tx_wrong_dur);
	}
	p_voice2->time = over_time;
	curvoice = p_voice2;
}