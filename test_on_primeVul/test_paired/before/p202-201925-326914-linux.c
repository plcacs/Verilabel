*vidtv_s302m_encoder_init(struct vidtv_s302m_encoder_init_args args)
{
	u32 priv_sz = sizeof(struct vidtv_s302m_ctx);
	struct vidtv_s302m_ctx *ctx;
	struct vidtv_encoder *e;

	e = kzalloc(sizeof(*e), GFP_KERNEL);
	if (!e)
		return NULL;

	e->id = S302M;

	if (args.name)
		e->name = kstrdup(args.name, GFP_KERNEL);

	e->encoder_buf = vzalloc(VIDTV_S302M_BUF_SZ);
	e->encoder_buf_sz = VIDTV_S302M_BUF_SZ;
	e->encoder_buf_offset = 0;

	e->sample_count = 0;

	e->src_buf = (args.src_buf) ? args.src_buf : NULL;
	e->src_buf_sz = (args.src_buf) ? args.src_buf_sz : 0;
	e->src_buf_offset = 0;

	e->is_video_encoder = false;

	ctx = kzalloc(priv_sz, GFP_KERNEL);
	if (!ctx) {
		kfree(e);
		return NULL;
	}

	e->ctx = ctx;
	ctx->last_duration = 0;

	e->encode = vidtv_s302m_encode;
	e->clear = vidtv_s302m_clear;

	e->es_pid = cpu_to_be16(args.es_pid);
	e->stream_id = cpu_to_be16(PES_PRIVATE_STREAM_1);

	e->sync = args.sync;
	e->sampling_rate_hz = S302M_SAMPLING_RATE_HZ;

	e->last_sample_cb = args.last_sample_cb;

	e->destroy = vidtv_s302m_encoder_destroy;

	if (args.head) {
		while (args.head->next)
			args.head = args.head->next;

		args.head->next = e;
	}

	e->next = NULL;

	return e;
}