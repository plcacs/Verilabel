static void handle_method_call(GDBusConnection *connection,
                        const gchar *caller,
                        const gchar *object_path,
                        const gchar *interface_name,
                        const gchar *method_name,
                        GVariant    *parameters,
                        GDBusMethodInvocation *invocation,
                        gpointer    user_data)
{
    reset_timeout();

    uid_t caller_uid;
    GVariant *response;

    caller_uid = get_caller_uid(connection, invocation, caller);

    log_notice("caller_uid:%ld method:'%s'", (long)caller_uid, method_name);

    if (caller_uid == (uid_t) -1)
        return;

    if (g_strcmp0(method_name, "NewProblem") == 0)
    {
        char *error = NULL;
        char *problem_id = handle_new_problem(g_variant_get_child_value(parameters, 0), caller_uid, &error);
        if (!problem_id)
        {
            g_dbus_method_invocation_return_dbus_error(invocation,
                                                      "org.freedesktop.problems.Failure",
                                                      error);
            free(error);
            return;
        }
        /* else */
        response = g_variant_new("(s)", problem_id);
        g_dbus_method_invocation_return_value(invocation, response);
        free(problem_id);

        return;
    }

    if (g_strcmp0(method_name, "GetProblems") == 0)
    {
        GList *dirs = get_problem_dirs_for_uid(caller_uid, g_settings_dump_location);
        response = variant_from_string_list(dirs);
        list_free_with_free(dirs);

        g_dbus_method_invocation_return_value(invocation, response);
        //I was told that g_dbus_method frees the response
        //g_variant_unref(response);
        return;
    }

    if (g_strcmp0(method_name, "GetAllProblems") == 0)
    {
        /*
        - so, we have UID,
        - if it's 0, then we don't have to check anything and just return all directories
        - if uid != 0 then we want to ask for authorization
        */
        if (caller_uid != 0)
        {
            if (polkit_check_authorization_dname(caller, "org.freedesktop.problems.getall") == PolkitYes)
                caller_uid = 0;
        }

        GList * dirs = get_problem_dirs_for_uid(caller_uid, g_settings_dump_location);
        response = variant_from_string_list(dirs);

        list_free_with_free(dirs);

        g_dbus_method_invocation_return_value(invocation, response);
        return;
    }

    if (g_strcmp0(method_name, "GetForeignProblems") == 0)
    {
        GList * dirs = get_problem_dirs_not_accessible_by_uid(caller_uid, g_settings_dump_location);
        response = variant_from_string_list(dirs);
        list_free_with_free(dirs);

        g_dbus_method_invocation_return_value(invocation, response);
        return;
    }

    if (g_strcmp0(method_name, "ChownProblemDir") == 0)
    {
        const gchar *problem_dir;
        g_variant_get(parameters, "(&s)", &problem_dir);
        log_notice("problem_dir:'%s'", problem_dir);

        if (!allowed_problem_dir(problem_dir))
        {
            return_InvalidProblemDir_error(invocation, problem_dir);
            return;
        }

        int dir_fd = dd_openfd(problem_dir);
        if (dir_fd < 0)
        {
            perror_msg("can't open problem directory '%s'", problem_dir);
            return_InvalidProblemDir_error(invocation, problem_dir);
            return;
        }

        int ddstat = fdump_dir_stat_for_uid(dir_fd, caller_uid);
        if (ddstat < 0)
        {
            if (errno == ENOTDIR)
            {
                log_notice("requested directory does not exist '%s'", problem_dir);
            }
            else
            {
                perror_msg("can't get stat of '%s'", problem_dir);
            }

            return_InvalidProblemDir_error(invocation, problem_dir);

            close(dir_fd);
            return;
        }

        if (ddstat & DD_STAT_OWNED_BY_UID)
        {   //caller seems to be in group with access to this dir, so no action needed
            log_notice("caller has access to the requested directory %s", problem_dir);
            g_dbus_method_invocation_return_value(invocation, NULL);
            close(dir_fd);
            return;
        }

        if ((ddstat & DD_STAT_ACCESSIBLE_BY_UID) == 0 &&
                polkit_check_authorization_dname(caller, "org.freedesktop.problems.getall") != PolkitYes)
        {
            log_notice("not authorized");
            g_dbus_method_invocation_return_dbus_error(invocation,
                                              "org.freedesktop.problems.AuthFailure",
                                              _("Not Authorized"));
            close(dir_fd);
            return;
        }

        struct dump_dir *dd = dd_fdopendir(dir_fd, problem_dir, DD_OPEN_READONLY | DD_FAIL_QUIETLY_EACCES);
        if (!dd)
        {
            return_InvalidProblemDir_error(invocation, problem_dir);
            return;
        }

        int chown_res = dd_chown(dd, caller_uid);
        if (chown_res != 0)
            g_dbus_method_invocation_return_dbus_error(invocation,
                                              "org.freedesktop.problems.ChownError",
                                              _("Chowning directory failed. Check system logs for more details."));
        else
            g_dbus_method_invocation_return_value(invocation, NULL);

        dd_close(dd);
        return;
    }

    if (g_strcmp0(method_name, "GetInfo") == 0)
    {
        /* Parameter tuple is (sas) */

	/* Get 1st param - problem dir name */
        const gchar *problem_dir;
        g_variant_get_child(parameters, 0, "&s", &problem_dir);
        log_notice("problem_dir:'%s'", problem_dir);

        if (!allowed_problem_dir(problem_dir))
        {
            return_InvalidProblemDir_error(invocation, problem_dir);
            return;
        }

        int dir_fd = dd_openfd(problem_dir);
        if (dir_fd < 0)
        {
            perror_msg("can't open problem directory '%s'", problem_dir);
            return_InvalidProblemDir_error(invocation, problem_dir);
            return;
        }

        if (!fdump_dir_accessible_by_uid(dir_fd, caller_uid))
        {
            if (errno == ENOTDIR)
            {
                log_notice("Requested directory does not exist '%s'", problem_dir);
                return_InvalidProblemDir_error(invocation, problem_dir);
                close(dir_fd);
                return;
            }

            if (polkit_check_authorization_dname(caller, "org.freedesktop.problems.getall") != PolkitYes)
            {
                log_notice("not authorized");
                g_dbus_method_invocation_return_dbus_error(invocation,
                                                  "org.freedesktop.problems.AuthFailure",
                                                  _("Not Authorized"));
                close(dir_fd);
                return;
            }
        }

        struct dump_dir *dd = dd_fdopendir(dir_fd, problem_dir, DD_OPEN_READONLY | DD_FAIL_QUIETLY_EACCES);
        if (!dd)
        {
            return_InvalidProblemDir_error(invocation, problem_dir);
            return;
        }

	/* Get 2nd param - vector of element names */
        GVariant *array = g_variant_get_child_value(parameters, 1);
        GList *elements = string_list_from_variant(array);
        g_variant_unref(array);

        GVariantBuilder *builder = NULL;
        for (GList *l = elements; l; l = l->next)
        {
            const char *element_name = (const char*)l->data;
            char *value = dd_load_text_ext(dd, element_name, 0
                                                | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE
                                                | DD_FAIL_QUIETLY_ENOENT
                                                | DD_FAIL_QUIETLY_EACCES);
            log_notice("element '%s' %s", element_name, value ? "fetched" : "not found");
            if (value)
            {
                if (!builder)
                    builder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);

                /* g_variant_builder_add makes a copy. No need to xstrdup here */
                g_variant_builder_add(builder, "{ss}", element_name, value);
                free(value);
            }
        }
        list_free_with_free(elements);
        dd_close(dd);
        /* It is OK to call g_variant_new("(a{ss})", NULL) because */
        /* G_VARIANT_TYPE_TUPLE allows NULL value */
        GVariant *response = g_variant_new("(a{ss})", builder);

        if (builder)
            g_variant_builder_unref(builder);

        log_info("GetInfo: returning value for '%s'", problem_dir);
        g_dbus_method_invocation_return_value(invocation, response);
        return;
    }

    if (g_strcmp0(method_name, "SetElement") == 0)
    {
        const char *problem_id;
        const char *element;
        const char *value;

        g_variant_get(parameters, "(&s&s&s)", &problem_id, &element, &value);

        if (!str_is_correct_filename(element))
        {
            log_notice("'%s' is not a valid element name of '%s'", element, problem_id);
            char *error = xasprintf(_("'%s' is not a valid element name"), element);
            g_dbus_method_invocation_return_dbus_error(invocation,
                                              "org.freedesktop.problems.InvalidElement",
                                              error);

            free(error);
            return;
        }

        struct dump_dir *dd = open_directory_for_modification_of_element(
                                    invocation, caller_uid, problem_id, element);
        if (!dd)
            /* Already logged from open_directory_for_modification_of_element() */
            return;

        /* Is it good idea to make it static? Is it possible to change the max size while a single run? */
        const double max_dir_size = g_settings_nMaxCrashReportsSize * (1024 * 1024);
        const long item_size = dd_get_item_size(dd, element);
        if (item_size < 0)
        {
            log_notice("Can't get size of '%s/%s'", problem_id, element);
            char *error = xasprintf(_("Can't get size of '%s'"), element);
            g_dbus_method_invocation_return_dbus_error(invocation,
                                                      "org.freedesktop.problems.Failure",
                                                      error);
            return;
        }

        const double requested_size = (double)strlen(value) - item_size;
        /* Don't want to check the size limit in case of reducing of size */
        if (requested_size > 0
            && requested_size > (max_dir_size - get_dirsize(g_settings_dump_location)))
        {
            log_notice("No problem space left in '%s' (requested Bytes %f)", problem_id, requested_size);
            g_dbus_method_invocation_return_dbus_error(invocation,
                                                      "org.freedesktop.problems.Failure",
                                                      _("No problem space left"));
        }
        else
        {
            dd_save_text(dd, element, value);
            g_dbus_method_invocation_return_value(invocation, NULL);
        }

        dd_close(dd);

        return;
    }

    if (g_strcmp0(method_name, "DeleteElement") == 0)
    {
        const char *problem_id;
        const char *element;

        g_variant_get(parameters, "(&s&s)", &problem_id, &element);

        if (!str_is_correct_filename(element))
        {
            log_notice("'%s' is not a valid element name of '%s'", element, problem_id);
            char *error = xasprintf(_("'%s' is not a valid element name"), element);
            g_dbus_method_invocation_return_dbus_error(invocation,
                                              "org.freedesktop.problems.InvalidElement",
                                              error);

            free(error);
            return;
        }

        struct dump_dir *dd = open_directory_for_modification_of_element(
                                    invocation, caller_uid, problem_id, element);
        if (!dd)
            /* Already logged from open_directory_for_modification_of_element() */
            return;

        const int res = dd_delete_item(dd, element);
        dd_close(dd);

        if (res != 0)
        {
            log_notice("Can't delete the element '%s' from the problem directory '%s'", element, problem_id);
            char *error = xasprintf(_("Can't delete the element '%s' from the problem directory '%s'"), element, problem_id);
            g_dbus_method_invocation_return_dbus_error(invocation,
                                          "org.freedesktop.problems.Failure",
                                          error);
            free(error);
            return;
        }


        g_dbus_method_invocation_return_value(invocation, NULL);
        return;
    }

    if (g_strcmp0(method_name, "DeleteProblem") == 0)
    {
        /* Dbus parameters are always tuples.
         * In this case, it's (as) - a tuple of one element (array of strings).
         * Need to fetch the array:
         */
        GVariant *array = g_variant_get_child_value(parameters, 0);
        GList *problem_dirs = string_list_from_variant(array);
        g_variant_unref(array);

        for (GList *l = problem_dirs; l; l = l->next)
        {
            const char *dir_name = (const char*)l->data;
            log_notice("dir_name:'%s'", dir_name);
            if (!allowed_problem_dir(dir_name))
            {
                return_InvalidProblemDir_error(invocation, dir_name);
                goto ret;
            }
        }

        for (GList *l = problem_dirs; l; l = l->next)
        {
            const char *dir_name = (const char*)l->data;

            int dir_fd = dd_openfd(dir_name);
            if (dir_fd < 0)
            {
                perror_msg("can't open problem directory '%s'", dir_name);
                return_InvalidProblemDir_error(invocation, dir_name);
                return;
            }

            if (!fdump_dir_accessible_by_uid(dir_fd, caller_uid))
            {
                if (errno == ENOTDIR)
                {
                    log_notice("Requested directory does not exist '%s'", dir_name);
                    close(dir_fd);
                    continue;
                }

                if (polkit_check_authorization_dname(caller, "org.freedesktop.problems.getall") != PolkitYes)
                { // if user didn't provide correct credentials, just move to the next dir
                    close(dir_fd);
                    continue;
                }
            }

            struct dump_dir *dd = dd_fdopendir(dir_fd, dir_name, /*flags:*/ 0);
            if (dd)
            {
                if (dd_delete(dd) != 0)
                {
                    error_msg("Failed to delete problem directory '%s'", dir_name);
                    dd_close(dd);
                }
            }
        }

        g_dbus_method_invocation_return_value(invocation, NULL);
 ret:
        list_free_with_free(problem_dirs);
        return;
    }

    if (g_strcmp0(method_name, "FindProblemByElementInTimeRange") == 0)
    {
        const gchar *element;
        const gchar *value;
        glong timestamp_from;
        glong timestamp_to;
        gboolean all;

        g_variant_get_child(parameters, 0, "&s", &element);
        g_variant_get_child(parameters, 1, "&s", &value);
        g_variant_get_child(parameters, 2, "x", &timestamp_from);
        g_variant_get_child(parameters, 3, "x", &timestamp_to);
        g_variant_get_child(parameters, 4, "b", &all);

        if (all && polkit_check_authorization_dname(caller, "org.freedesktop.problems.getall") == PolkitYes)
            caller_uid = 0;

        GList *dirs = get_problem_dirs_for_element_in_time(caller_uid, element, value, timestamp_from,
                                                        timestamp_to);
        response = variant_from_string_list(dirs);
        list_free_with_free(dirs);

        g_dbus_method_invocation_return_value(invocation, response);
        return;
    }

    if (g_strcmp0(method_name, "Quit") == 0)
    {
        g_dbus_method_invocation_return_value(invocation, NULL);
        g_main_loop_quit(loop);
        return;
    }
}