int go7007_snd_init(struct go7007 *go)
{
	static int dev;
	struct go7007_snd *gosnd;
	int ret;

	if (dev >= SNDRV_CARDS)
		return -ENODEV;
	if (!enable[dev]) {
		dev++;
		return -ENOENT;
	}
	gosnd = kmalloc(sizeof(struct go7007_snd), GFP_KERNEL);
	if (gosnd == NULL)
		return -ENOMEM;
	spin_lock_init(&gosnd->lock);
	gosnd->hw_ptr = gosnd->w_idx = gosnd->avail = 0;
	gosnd->capturing = 0;
	ret = snd_card_new(go->dev, index[dev], id[dev], THIS_MODULE, 0,
			   &gosnd->card);
	if (ret < 0)
		goto free_snd;

	ret = snd_device_new(gosnd->card, SNDRV_DEV_LOWLEVEL, go,
			&go7007_snd_device_ops);
	if (ret < 0)
		goto free_card;

	ret = snd_pcm_new(gosnd->card, "go7007", 0, 0, 1, &gosnd->pcm);
	if (ret < 0)
		goto free_card;

	strscpy(gosnd->card->driver, "go7007", sizeof(gosnd->card->driver));
	strscpy(gosnd->card->shortname, go->name, sizeof(gosnd->card->shortname));
	strscpy(gosnd->card->longname, gosnd->card->shortname,
		sizeof(gosnd->card->longname));

	gosnd->pcm->private_data = go;
	snd_pcm_set_ops(gosnd->pcm, SNDRV_PCM_STREAM_CAPTURE,
			&go7007_snd_capture_ops);

	ret = snd_card_register(gosnd->card);
	if (ret < 0)
		goto free_card;

	gosnd->substream = NULL;
	go->snd_context = gosnd;
	v4l2_device_get(&go->v4l2_dev);
	++dev;

	return 0;

free_card:
	snd_card_free(gosnd->card);
free_snd:
	kfree(gosnd);
	return ret;
}