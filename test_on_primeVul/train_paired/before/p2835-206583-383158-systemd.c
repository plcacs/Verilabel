static int acquire_security_info(sd_bus *bus, const char *name, struct security_info *info, AnalyzeSecurityFlags flags) {

        static const struct bus_properties_map security_map[] = {
                { "AmbientCapabilities",     "t",       NULL,                                    offsetof(struct security_info, ambient_capabilities)      },
                { "CapabilityBoundingSet",   "t",       NULL,                                    offsetof(struct security_info, capability_bounding_set)   },
                { "DefaultDependencies",     "b",       NULL,                                    offsetof(struct security_info, default_dependencies)      },
                { "Delegate",                "b",       NULL,                                    offsetof(struct security_info, delegate)                  },
                { "DeviceAllow",             "a(ss)",   property_read_device_allow,              0                                                         },
                { "DevicePolicy",            "s",       NULL,                                    offsetof(struct security_info, device_policy)             },
                { "DynamicUser",             "b",       NULL,                                    offsetof(struct security_info, dynamic_user)              },
                { "FragmentPath",            "s",       NULL,                                    offsetof(struct security_info, fragment_path)             },
                { "IPAddressAllow",          "a(iayu)", property_read_ip_address_allow,          0                                                         },
                { "IPAddressDeny",           "a(iayu)", property_read_ip_address_allow,          0                                                         },
                { "Id",                      "s",       NULL,                                    offsetof(struct security_info, id)                        },
                { "KeyringMode",             "s",       NULL,                                    offsetof(struct security_info, keyring_mode)              },
                { "LoadState",               "s",       NULL,                                    offsetof(struct security_info, load_state)                },
                { "LockPersonality",         "b",       NULL,                                    offsetof(struct security_info, lock_personality)          },
                { "MemoryDenyWriteExecute",  "b",       NULL,                                    offsetof(struct security_info, memory_deny_write_execute) },
                { "NoNewPrivileges",         "b",       NULL,                                    offsetof(struct security_info, no_new_privileges)         },
                { "NotifyAccess",            "s",       NULL,                                    offsetof(struct security_info, notify_access)             },
                { "PrivateDevices",          "b",       NULL,                                    offsetof(struct security_info, private_devices)           },
                { "PrivateMounts",           "b",       NULL,                                    offsetof(struct security_info, private_mounts)            },
                { "PrivateNetwork",          "b",       NULL,                                    offsetof(struct security_info, private_network)           },
                { "PrivateTmp",              "b",       NULL,                                    offsetof(struct security_info, private_tmp)               },
                { "PrivateUsers",            "b",       NULL,                                    offsetof(struct security_info, private_users)             },
                { "ProtectControlGroups",    "b",       NULL,                                    offsetof(struct security_info, protect_control_groups)    },
                { "ProtectHome",             "s",       NULL,                                    offsetof(struct security_info, protect_home)              },
                { "ProtectHostname",         "b",       NULL,                                    offsetof(struct security_info, protect_hostname)          },
                { "ProtectKernelModules",    "b",       NULL,                                    offsetof(struct security_info, protect_kernel_modules)    },
                { "ProtectKernelTunables",   "b",       NULL,                                    offsetof(struct security_info, protect_kernel_tunables)   },
                { "ProtectSystem",           "s",       NULL,                                    offsetof(struct security_info, protect_system)            },
                { "RemoveIPC",               "b",       NULL,                                    offsetof(struct security_info, remove_ipc)                },
                { "RestrictAddressFamilies", "(bas)",   property_read_restrict_address_families, 0                                                         },
                { "RestrictNamespaces",      "t",       NULL,                                    offsetof(struct security_info, restrict_namespaces)       },
                { "RestrictRealtime",        "b",       NULL,                                    offsetof(struct security_info, restrict_realtime)         },
                { "RootDirectory",           "s",       NULL,                                    offsetof(struct security_info, root_directory)            },
                { "RootImage",               "s",       NULL,                                    offsetof(struct security_info, root_image)                },
                { "SupplementaryGroups",     "as",      NULL,                                    offsetof(struct security_info, supplementary_groups)      },
                { "SystemCallArchitectures", "as",      NULL,                                    offsetof(struct security_info, system_call_architectures) },
                { "SystemCallFilter",        "(as)",    property_read_system_call_filter,        0                                                         },
                { "Type",                    "s",       NULL,                                    offsetof(struct security_info, type)                      },
                { "UMask",                   "u",       NULL,                                    offsetof(struct security_info, _umask)                    },
                { "User",                    "s",       NULL,                                    offsetof(struct security_info, user)                      },
                {}
        };

        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        _cleanup_free_ char *path = NULL;
        int r;

        /* Note: this mangles *info on failure! */

        assert(bus);
        assert(name);
        assert(info);

        path = unit_dbus_path_from_name(name);
        if (!path)
                return log_oom();

        r = bus_map_all_properties(bus,
                                   "org.freedesktop.systemd1",
                                   path,
                                   security_map,
                                   BUS_MAP_STRDUP|BUS_MAP_BOOLEAN_AS_BOOL,
                                   &error,
                                   NULL,
                                   info);
        if (r < 0)
                return log_error_errno(r, "Failed to get unit properties: %s", bus_error_message(&error, r));

        if (!streq_ptr(info->load_state, "loaded")) {

                if (FLAGS_SET(flags, ANALYZE_SECURITY_ONLY_LOADED))
                        return -EMEDIUMTYPE;

                if (streq_ptr(info->load_state, "not-found"))
                        log_error("Unit %s not found, cannot analyze.", name);
                else if (streq_ptr(info->load_state, "masked"))
                        log_error("Unit %s is masked, cannot analyze.", name);
                else
                        log_error("Unit %s not loaded properly, cannot analyze.", name);

                return -EINVAL;
        }

        if (FLAGS_SET(flags, ANALYZE_SECURITY_ONLY_LONG_RUNNING) && streq_ptr(info->type, "oneshot"))
                return -EMEDIUMTYPE;

        if (info->private_devices ||
            info->private_tmp ||
            info->protect_control_groups ||
            info->protect_kernel_tunables ||
            info->protect_kernel_modules ||
            !streq_ptr(info->protect_home, "no") ||
            !streq_ptr(info->protect_system, "no") ||
            info->root_image)
                info->private_mounts = true;

        if (info->protect_kernel_modules)
                info->capability_bounding_set &= ~(UINT64_C(1) << CAP_SYS_MODULE);

        if (info->private_devices)
                info->capability_bounding_set &= ~((UINT64_C(1) << CAP_MKNOD) |
                                                   (UINT64_C(1) << CAP_SYS_RAWIO));

        return 0;
}