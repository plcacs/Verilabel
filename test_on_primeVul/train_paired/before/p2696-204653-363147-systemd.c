int unit_patch_contexts(Unit *u) {
        CGroupContext *cc;
        ExecContext *ec;
        unsigned i;
        int r;

        assert(u);

        /* Patch in the manager defaults into the exec and cgroup
         * contexts, _after_ the rest of the settings have been
         * initialized */

        ec = unit_get_exec_context(u);
        if (ec) {
                /* This only copies in the ones that need memory */
                for (i = 0; i < _RLIMIT_MAX; i++)
                        if (u->manager->rlimit[i] && !ec->rlimit[i]) {
                                ec->rlimit[i] = newdup(struct rlimit, u->manager->rlimit[i], 1);
                                if (!ec->rlimit[i])
                                        return -ENOMEM;
                        }

                if (MANAGER_IS_USER(u->manager) &&
                    !ec->working_directory) {

                        r = get_home_dir(&ec->working_directory);
                        if (r < 0)
                                return r;

                        /* Allow user services to run, even if the
                         * home directory is missing */
                        ec->working_directory_missing_ok = true;
                }

                if (ec->private_devices)
                        ec->capability_bounding_set &= ~((UINT64_C(1) << CAP_MKNOD) | (UINT64_C(1) << CAP_SYS_RAWIO));

                if (ec->protect_kernel_modules)
                        ec->capability_bounding_set &= ~(UINT64_C(1) << CAP_SYS_MODULE);

                if (ec->dynamic_user) {
                        if (!ec->user) {
                                r = user_from_unit_name(u, &ec->user);
                                if (r < 0)
                                        return r;
                        }

                        if (!ec->group) {
                                ec->group = strdup(ec->user);
                                if (!ec->group)
                                        return -ENOMEM;
                        }

                        /* If the dynamic user option is on, let's make sure that the unit can't leave its UID/GID
                         * around in the file system or on IPC objects. Hence enforce a strict sandbox. */

                        ec->private_tmp = true;
                        ec->remove_ipc = true;
                        ec->protect_system = PROTECT_SYSTEM_STRICT;
                        if (ec->protect_home == PROTECT_HOME_NO)
                                ec->protect_home = PROTECT_HOME_READ_ONLY;
                }
        }

        cc = unit_get_cgroup_context(u);
        if (cc && ec) {

                if (ec->private_devices &&
                    cc->device_policy == CGROUP_AUTO)
                        cc->device_policy = CGROUP_CLOSED;

                if (ec->root_image &&
                    (cc->device_policy != CGROUP_AUTO || cc->device_allow)) {

                        /* When RootImage= is specified, the following devices are touched. */
                        r = cgroup_add_device_allow(cc, "/dev/loop-control", "rw");
                        if (r < 0)
                                return r;

                        r = cgroup_add_device_allow(cc, "block-loop", "rwm");
                        if (r < 0)
                                return r;

                        r = cgroup_add_device_allow(cc, "block-blkext", "rwm");
                        if (r < 0)
                                return r;
                }
        }

        return 0;
}