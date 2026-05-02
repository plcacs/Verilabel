static GF_Err BM_ParseGlobalQuantizer(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	GF_Node *node;
	GF_Command *com;
	GF_CommandField *inf;
	node = gf_bifs_dec_node(codec, bs, NDT_SFWorldNode);
	if (!node) return GF_NON_COMPLIANT_BITSTREAM;

	/*reset global QP*/
	if (codec->scenegraph->global_qp) {
		gf_node_unregister(codec->scenegraph->global_qp, NULL);
	}
	codec->ActiveQP = NULL;
	codec->scenegraph->global_qp = NULL;

	if (gf_node_get_tag(node) != TAG_MPEG4_QuantizationParameter) {
		gf_node_unregister(node, NULL);
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	/*register global QP*/
	codec->ActiveQP = (M_QuantizationParameter *) node;
	codec->ActiveQP->isLocal = 0;
	codec->scenegraph->global_qp = node;

	/*register TWICE: once for the command, and for the scenegraph globalQP*/
	node->sgprivate->num_instances = 2;

	com = gf_sg_command_new(codec->current_graph, GF_SG_GLOBAL_QUANTIZER);
	inf = gf_sg_command_field_new(com);
	inf->new_node = node;
	inf->field_ptr = &inf->new_node;
	inf->fieldType = GF_SG_VRML_SFNODE;
	gf_list_add(com_list, com);
	return GF_OK;
}