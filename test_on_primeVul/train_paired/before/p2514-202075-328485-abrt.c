static int SavePackageDescriptionToDebugDump(const char *dump_dir_name)
{
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;

    char *analyzer = dd_load_text(dd, FILENAME_ANALYZER);
    if (!strcmp(analyzer, "Kerneloops"))
    {
        dd_save_text(dd, FILENAME_PACKAGE, "kernel");
        dd_save_text(dd, FILENAME_COMPONENT, "kernel");
        dd_close(dd);
        free(analyzer);
        return 0;
    }
    free(analyzer);

    char *cmdline = NULL;
    char *executable = NULL;
    char *rootdir = NULL;
    char *package_short_name = NULL;
    struct pkg_envra *pkg_name = NULL;
    char *component = NULL;
    int error = 1;
    /* note: "goto ret" statements below free all the above variables,
     * but they don't dd_close(dd) */

    cmdline = dd_load_text_ext(dd, FILENAME_CMDLINE, DD_FAIL_QUIETLY_ENOENT);
    executable = dd_load_text(dd, FILENAME_EXECUTABLE);
    rootdir = dd_load_text_ext(dd, FILENAME_ROOTDIR,
                               DD_FAIL_QUIETLY_ENOENT | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);

    /* Close dd while we query package database. It can take some time,
     * don't want to keep dd locked longer than necessary */
    dd_close(dd);

    if (is_path_blacklisted(executable))
    {
        log("Blacklisted executable '%s'", executable);
        goto ret; /* return 1 (failure) */
    }

    pkg_name = rpm_get_package_nvr(executable, rootdir);
    if (!pkg_name)
    {
        if (settings_bProcessUnpackaged)
        {
            log_info("Crash in unpackaged executable '%s', "
                      "proceeding without packaging information", executable);
            goto ret0; /* no error */
        }
        log("Executable '%s' doesn't belong to any package"
		" and ProcessUnpackaged is set to 'no'",
		executable
        );
        goto ret; /* return 1 (failure) */
    }

    /* Check well-known interpreter names */
    const char *basename = strrchr(executable, '/');
    if (basename)
        basename++;
    else
        basename = executable;

    /* if basename is known interpreter, we want to blame the running script
     * not the interpreter
     */
    if (g_list_find_custom(settings_Interpreters, basename, (GCompareFunc)g_strcmp0))
    {
        struct pkg_envra *script_pkg = get_script_name(cmdline, &executable);
        /* executable may have changed, check it again */
        if (is_path_blacklisted(executable))
        {
            log("Blacklisted executable '%s'", executable);
            goto ret; /* return 1 (failure) */
        }
        if (!script_pkg)
        {
            /* Script name is not absolute, or it doesn't
             * belong to any installed package.
             */
            if (!settings_bProcessUnpackaged)
            {
                log("Interpreter crashed, but no packaged script detected: '%s'", cmdline);
                goto ret; /* return 1 (failure) */
            }

            /* Unpackaged script, but the settings says we want to keep it.
             * BZ plugin wont allow to report this anyway, because component
             * is missing, so there is no reason to mark it as not_reportable.
             * Someone might want to use abrt to report it using ftp.
             */
            goto ret0;
        }

        free_pkg_envra(pkg_name);
        pkg_name = script_pkg;
    }

    package_short_name = xasprintf("%s", pkg_name->p_name);
    log_info("Package:'%s' short:'%s'", pkg_name->p_nvr, package_short_name);


    if (g_list_find_custom(settings_setBlackListedPkgs, package_short_name, (GCompareFunc)g_strcmp0))
    {
        log("Blacklisted package '%s'", package_short_name);
        goto ret; /* return 1 (failure) */
    }

    if (settings_bOpenGPGCheck)
    {
        if (!rpm_chk_fingerprint(package_short_name))
        {
            log("Package '%s' isn't signed with proper key", package_short_name);
            goto ret; /* return 1 (failure) */
        }
        /* We used to also check the integrity of the executable here:
         *  if (!CheckHash(package_short_name.c_str(), executable)) BOOM();
         * Checking the MD5 sum requires to run prelink to "un-prelink" the
         * binaries - this is considered potential security risk so we don't
         * do it now, until we find some non-intrusive way.
         */
    }

    component = rpm_get_component(executable, rootdir);

    dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        goto ret; /* return 1 (failure) */

    if (pkg_name)
    {
        dd_save_text(dd, FILENAME_PACKAGE, pkg_name->p_nvr);
        dd_save_text(dd, FILENAME_PKG_EPOCH, pkg_name->p_epoch);
        dd_save_text(dd, FILENAME_PKG_NAME, pkg_name->p_name);
        dd_save_text(dd, FILENAME_PKG_VERSION, pkg_name->p_version);
        dd_save_text(dd, FILENAME_PKG_RELEASE, pkg_name->p_release);
        dd_save_text(dd, FILENAME_PKG_ARCH, pkg_name->p_arch);
    }

    if (component)
        dd_save_text(dd, FILENAME_COMPONENT, component);

    dd_close(dd);

 ret0:
    error = 0;
 ret:
    free(cmdline);
    free(executable);
    free(rootdir);
    free(package_short_name);
    free_pkg_envra(pkg_name);
    free(component);

    return error;
}