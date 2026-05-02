void CleanWriters(GF_List *writers)
{
	while (gf_list_count(writers)) {
		TrackWriter *writer = (TrackWriter*)gf_list_get(writers, 0);
		gf_isom_box_del(writer->stco);
		gf_isom_box_del((GF_Box *)writer->stsc);
		gf_free(writer);
		gf_list_rem(writers, 0);
	}
}