static void cil_reset_classpermissionset(struct cil_classpermissionset *cps)
{
	cil_reset_classperms_list(cps->classperms);
}