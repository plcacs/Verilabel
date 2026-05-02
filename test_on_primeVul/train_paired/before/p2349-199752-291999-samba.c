static bool acl_group_override(connection_struct *conn,
				gid_t prim_gid,
				const char *fname)
{
	SMB_STRUCT_STAT sbuf;

	if ((errno != EPERM) && (errno != EACCES)) {
		return false;
	}

	/* file primary group == user primary or supplementary group */
	if (lp_acl_group_control(SNUM(conn)) &&
			current_user_in_group(prim_gid)) {
		return true;
	}

	/* user has writeable permission */
	if (lp_dos_filemode(SNUM(conn)) &&
			can_write_to_file(conn, fname, &sbuf)) {
		return true;
	}

	return false;
}