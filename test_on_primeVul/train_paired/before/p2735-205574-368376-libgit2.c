static int checkout_verify_paths(
	git_repository *repo,
	int action,
	git_diff_delta *delta)
{
	unsigned int flags = GIT_PATH_REJECT_WORKDIR_DEFAULTS;

	if (action & CHECKOUT_ACTION__REMOVE) {
		if (!git_path_isvalid(repo, delta->old_file.path, delta->old_file.mode, flags)) {
			git_error_set(GIT_ERROR_CHECKOUT, "cannot remove invalid path '%s'", delta->old_file.path);
			return -1;
		}
	}

	if (action & ~CHECKOUT_ACTION__REMOVE) {
		if (!git_path_isvalid(repo, delta->new_file.path, delta->new_file.mode, flags)) {
			git_error_set(GIT_ERROR_CHECKOUT, "cannot checkout to invalid path '%s'", delta->new_file.path);
			return -1;
		}
	}

	return 0;
}