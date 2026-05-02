static unsigned long get_ctl_id_hash(const struct snd_ctl_elem_id *id)
{
	int i;
	unsigned long h;

	h = id->iface;
	h = MULTIPLIER * h + id->device;
	h = MULTIPLIER * h + id->subdevice;
	for (i = 0; id->name[i] && i < SNDRV_CTL_ELEM_ID_NAME_MAXLEN; i++)
		h = MULTIPLIER * h + id->name[i];
	h = MULTIPLIER * h + id->index;
	h &= LONG_MAX;
	return h;
}