int main(int argc, char *argv[])
{
	int fd, n, pid, request, ret;
	char *me, *newname;
	struct user_nic_args args;
	int container_veth_ifidx = -1, host_veth_ifidx = -1, netns_fd = -1;
	char *cnic = NULL, *nicname = NULL;
	struct alloted_s *alloted = NULL;

	if (argc < 7 || argc > 8) {
		usage(argv[0], true);
		exit(EXIT_FAILURE);
	}

	memset(&args, 0, sizeof(struct user_nic_args));
	args.cmd = argv[1];
	args.lxc_path = argv[2];
	args.lxc_name = argv[3];
	args.pid = argv[4];
	args.type = argv[5];
	args.link = argv[6];
	if (argc >= 8)
		args.veth_name = argv[7];

	if (!strcmp(args.cmd, "create")) {
		request = LXC_USERNIC_CREATE;
	} else if (!strcmp(args.cmd, "delete")) {
		request = LXC_USERNIC_DELETE;
	} else {
		usage(argv[0], true);
		exit(EXIT_FAILURE);
	}

	/* Set a sane env, because we are setuid-root. */
	ret = clearenv();
	if (ret) {
		usernic_error("%s", "Failed to clear environment\n");
		exit(EXIT_FAILURE);
	}

	ret = setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);
	if (ret < 0) {
		usernic_error("%s", "Failed to set PATH, exiting\n");
		exit(EXIT_FAILURE);
	}

	me = get_username();
	if (!me) {
		usernic_error("%s", "Failed to get username\n");
		exit(EXIT_FAILURE);
	}

	if (request == LXC_USERNIC_CREATE) {
		ret = lxc_safe_int(args.pid, &pid);
		if (ret < 0) {
			usernic_error("Could not read pid: %s\n", args.pid);
			exit(EXIT_FAILURE);
		}
	} else if (request == LXC_USERNIC_DELETE) {
		netns_fd = open(args.pid, O_RDONLY);
		if (netns_fd < 0) {
			usernic_error("Could not open \"%s\": %s\n", args.pid,
				      strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	if (!create_db_dir(LXC_USERNIC_DB)) {
		usernic_error("%s", "Failed to create directory for db file\n");
		if (netns_fd >= 0)
			close(netns_fd);
		exit(EXIT_FAILURE);
	}

	fd = open_and_lock(LXC_USERNIC_DB);
	if (fd < 0) {
		usernic_error("Failed to lock %s\n", LXC_USERNIC_DB);
		if (netns_fd >= 0)
			close(netns_fd);
		exit(EXIT_FAILURE);
	}

	if (request == LXC_USERNIC_CREATE) {
		if (!may_access_netns(pid)) {
			usernic_error("User %s may not modify netns for pid %d\n", me, pid);
			exit(EXIT_FAILURE);
		}
	} else if (request == LXC_USERNIC_DELETE) {
		bool has_priv;
		has_priv = is_privileged_over_netns(netns_fd);
		close(netns_fd);
		if (!has_priv) {
			usernic_error("%s", "Process is not privileged over "
					    "network namespace\n");
			exit(EXIT_FAILURE);
		}
	}

	n = get_alloted(me, args.type, args.link, &alloted);

	if (request == LXC_USERNIC_DELETE) {
		int ret;
		struct alloted_s *it;
		bool found_nicname = false;

		if (!is_ovs_bridge(args.link)) {
			usernic_error("%s", "Deletion of non ovs type network "
					    "devices not implemented\n");
			close(fd);
			free_alloted(&alloted);
			exit(EXIT_FAILURE);
		}

		/* Check whether the network device we are supposed to delete
		 * exists in the db. If it doesn't we will not delete it as we
		 * need to assume the network device is not under our control.
		 * As a side effect we also clear any invalid entries from the
		 * database.
		 */
		for (it = alloted; it; it = it->next)
			cull_entries(fd, it->name, args.type, args.link,
				     args.veth_name, &found_nicname);
		close(fd);
		free_alloted(&alloted);

		if (!found_nicname) {
			usernic_error("Caller is not allowed to delete network "
				      "device \"%s\"\n", args.veth_name);
			exit(EXIT_FAILURE);
		}

		ret = lxc_ovs_delete_port(args.link, args.veth_name);
		if (ret < 0) {
			usernic_error("Failed to remove port \"%s\" from "
				      "openvswitch bridge \"%s\"",
				      args.veth_name, args.link);
			exit(EXIT_FAILURE);
		}

		exit(EXIT_SUCCESS);
	}
	if (n > 0)
		nicname = get_nic_if_avail(fd, alloted, pid, args.type,
					   args.link, n, &cnic);

	close(fd);
	free_alloted(&alloted);
	if (!nicname) {
		usernic_error("%s", "Quota reached\n");
		exit(EXIT_FAILURE);
	}

	/* Now rename the link. */
	newname = lxc_secure_rename_in_ns(pid, cnic, args.veth_name,
					  &container_veth_ifidx);
	if (!newname) {
		usernic_error("%s", "Failed to rename the link\n");
		ret = lxc_netdev_delete_by_name(cnic);
		if (ret < 0)
			usernic_error("Failed to delete \"%s\"\n", cnic);
		free(nicname);
		exit(EXIT_FAILURE);
	}

	host_veth_ifidx = if_nametoindex(nicname);
	if (!host_veth_ifidx) {
		free(newname);
		free(nicname);
		usernic_error("Failed to get netdev index: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Write names of veth pairs and their ifindeces to stout:
	 * (e.g. eth0:731:veth9MT2L4:730)
	 */
	fprintf(stdout, "%s:%d:%s:%d\n", newname, container_veth_ifidx, nicname,
		host_veth_ifidx);
	free(newname);
	free(nicname);
	exit(EXIT_SUCCESS);
}