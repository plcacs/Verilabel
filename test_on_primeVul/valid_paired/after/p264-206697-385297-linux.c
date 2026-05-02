static void umd_cleanup(struct subprocess_info *info)
{
	struct umd_info *umd_info = info->data;

	/* cleanup if umh_setup() was successful but exec failed */
	if (info->retval)
		umd_cleanup_helper(umd_info);
}