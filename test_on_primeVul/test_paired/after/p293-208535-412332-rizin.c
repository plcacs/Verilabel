static RzList *maps(RzBinFile *bf) {
	rz_return_val_if_fail(bf && bf->o, NULL);
	QnxObj *qo = bf->o->bin_obj;
	return rz_list_clone(qo->maps);
}