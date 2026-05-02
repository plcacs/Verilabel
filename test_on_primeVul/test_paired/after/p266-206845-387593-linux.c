static unsigned long get_ctl_id_hash(const struct snd_ctl_elem_id *id)
{
	int i;
	unsigned long h;

	h = id->iface;
	h = MULTIPLIER * h + id->device;
	h = MULTIPLIER * h + id->subdevice;
	for (i = 0; i < SNDRV_CTL_ELEM_ID_NAME_MAXLEN && id->name[i]; i++)
		h = MULTIPLIER * h + id->name[i];
	h = MULTIPLIER * h + id->index;
	h &= LONG_MAX;
	return h;
}