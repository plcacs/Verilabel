void CleanWriters(GF_List *writers)
{
	while (gf_list_count(writers)) {
		TrackWriter *writer = (TrackWriter*)gf_list_get(writers, 0);
		//in case we have an error in the middle of file write, remove our created stco and stsc from sample table
		gf_list_del_item(writer->stbl->child_boxes, writer->stco);
		gf_list_del_item(writer->stbl->child_boxes, writer->stsc);
		gf_isom_box_del(writer->stco);
		gf_isom_box_del((GF_Box *)writer->stsc);
		gf_free(writer);
		gf_list_rem(writers, 0);
	}
}