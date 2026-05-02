static void gf_m2ts_process_pat(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_ES *ses, GF_List *sections, u8 table_id, u16 ex_table_id, u8 version_number, u8 last_section_number, u32 status)
{
	GF_M2TS_Program *prog;
	GF_M2TS_SECTION_ES *pmt;
	u32 i, nb_progs, evt_type;
	u32 nb_sections;
	u32 data_size;
	unsigned char *data;
	GF_M2TS_Section *section;

	/*wait for the last section */
	if (!(status&GF_M2TS_TABLE_END)) return;

	/*skip if already received*/
	if (status&GF_M2TS_TABLE_REPEAT) {
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_PAT_REPEAT, NULL);
		return;
	}

	nb_sections = gf_list_count(sections);
	if (nb_sections > 1) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("PAT on multiple sections not supported\n"));
	}

	section = (GF_M2TS_Section *)gf_list_get(sections, 0);
	data = section->data;
	data_size = section->data_size;

	if (!(status&GF_M2TS_TABLE_UPDATE) && gf_list_count(ts->programs)) {
		if (ts->pat->demux_restarted) {
			ts->pat->demux_restarted = 0;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Multiple different PAT on single TS found, ignoring new PAT declaration (table id %d - extended table id %d)\n", table_id, ex_table_id));
		}
		return;
	}
	nb_progs = data_size / 4;

	for (i=0; i<nb_progs; i++) {
		u16 number, pid;
		number = (data[0]<<8) | data[1];
		pid = (data[2]&0x1f)<<8 | data[3];
		data += 4;
		if (number==0) {
			if (!ts->nit) {
				ts->nit = gf_m2ts_section_filter_new(gf_m2ts_process_nit, 0);
			}
		} else {
			GF_SAFEALLOC(prog, GF_M2TS_Program);
			if (!prog) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Fail to allocate program for pid %d\n", pid));
				return;
			}
			prog->streams = gf_list_new();
			prog->pmt_pid = pid;
			prog->number = number;
			prog->ts = ts;
			gf_list_add(ts->programs, prog);
			GF_SAFEALLOC(pmt, GF_M2TS_SECTION_ES);
			if (!pmt) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Fail to allocate pmt filter for pid %d\n", pid));
				return;
			}
			pmt->flags = GF_M2TS_ES_IS_SECTION;
			gf_list_add(prog->streams, pmt);
			pmt->pid = prog->pmt_pid;
			pmt->program = prog;
			ts->ess[pmt->pid] = (GF_M2TS_ES *)pmt;
			pmt->sec = gf_m2ts_section_filter_new(gf_m2ts_process_pmt, 0);
		}
	}

	evt_type = (status&GF_M2TS_TABLE_UPDATE) ? GF_M2TS_EVT_PAT_UPDATE : GF_M2TS_EVT_PAT_FOUND;
	if (ts->on_event) ts->on_event(ts, evt_type, NULL);
}