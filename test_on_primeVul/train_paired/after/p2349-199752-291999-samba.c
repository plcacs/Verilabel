static bool acl_group_override(connection_struct *conn,
				SMB_STRUCT_STAT *psbuf,
				const char *fname)
{
	if ((errno != EPERM) && (errno != EACCES)) {
		return false;
	}

	/* file primary group == user primary or supplementary group */
	if (lp_acl_group_control(SNUM(conn)) &&
			current_user_in_group(psbuf->st_gid)) {
		return true;
	}

	/* user has writeable permission */
	if (lp_dos_filemode(SNUM(conn)) &&
			can_write_to_file(conn, fname, psbuf)) {
		return true;
	}

	return false;
}