int callback_glewlwyd_user_auth (const struct _u_request * request, struct _u_response * response, void * user_data) {
  struct config_elements * config = (struct config_elements *)user_data;
  json_t * j_param = ulfius_get_json_body_request(request, NULL), * j_result = NULL;
  const char * ip_source = get_ip_source(request);
  char * issued_for = get_client_hostname(request);
  char * session_uid, expires[129];
  time_t now;
  struct tm ts;
  
  time(&now);
  now += GLEWLWYD_DEFAULT_SESSION_EXPIRATION_COOKIE;
  gmtime_r(&now, &ts);
  strftime(expires, 128, "%a, %d %b %Y %T %Z", &ts);
  if (j_param != NULL) {
    if (json_string_length(json_object_get(j_param, "username"))) {
      if (json_object_get(j_param, "scheme_type") == NULL || 0 == o_strcmp(json_string_value(json_object_get(j_param, "scheme_type")), "password")) {
        if (json_string_length(json_object_get(j_param, "password"))) {
          j_result = auth_check_user_credentials(config, json_string_value(json_object_get(j_param, "username")), json_string_value(json_object_get(j_param, "password")));
          if (check_result_value(j_result, G_OK)) {
            if ((session_uid = get_session_id(config, request)) == NULL) {
              session_uid = generate_session_id();
            }
            if (user_session_update(config, session_uid, u_map_get_case(request->map_header, "user-agent"), issued_for, json_string_value(json_object_get(j_param, "username")), NULL, 1) != G_OK) {
              y_log_message(Y_LOG_LEVEL_ERROR, "callback_glewlwyd_user_auth - Error user_session_update (1)");
              response->status = 500;
            } else {
              ulfius_add_cookie_to_response(response, config->session_key, session_uid, expires, 0, config->cookie_domain, "/", config->cookie_secure, 0);
              y_log_message(Y_LOG_LEVEL_INFO, "Event - User '%s' authenticated with password", json_string_value(json_object_get(j_param, "username")));
            }
            o_free(session_uid);
            glewlwyd_metrics_increment_counter_va(config, GLWD_METRICS_AUTH_USER_VALID, 1, NULL);
            glewlwyd_metrics_increment_counter_va(config, GLWD_METRICS_AUTH_USER_VALID_SCHEME, 1, "scheme_type", "password", NULL);
          } else {
            if (check_result_value(j_result, G_ERROR_UNAUTHORIZED)) {
              y_log_message(Y_LOG_LEVEL_WARNING, "Security - Authorization invalid for username %s at IP Address %s", json_string_value(json_object_get(j_param, "username")), ip_source);
            }
            response->status = 401;
            glewlwyd_metrics_increment_counter_va(config, GLWD_METRICS_AUTH_USER_INVALID, 1, NULL);
            glewlwyd_metrics_increment_counter_va(config, GLWD_METRICS_AUTH_USER_INVALID_SCHEME, 1, "scheme_type", "password", NULL);
          }
          json_decref(j_result);
        } else if (json_object_get(j_param, "password") != NULL && !json_is_string(json_object_get(j_param, "password"))) {
          ulfius_set_string_body_response(response, 400, "password must be a string");
        } else {
          session_uid = get_session_id(config, request);
          j_result = get_users_for_session(config, session_uid);
          if (check_result_value(j_result, G_OK)) {
            // Refresh username to set as default
            if (user_session_update(config, u_map_get(request->map_cookie, config->session_key), u_map_get_case(request->map_header, "user-agent"), issued_for, json_string_value(json_object_get(j_param, "username")), NULL, 0) != G_OK) {
              y_log_message(Y_LOG_LEVEL_ERROR, "callback_glewlwyd_user_auth - Error user_session_update (3)");
              response->status = 500;
            } else {
              ulfius_add_cookie_to_response(response, config->session_key, session_uid, expires, 0, config->cookie_domain, "/", config->cookie_secure, 0);
            }
          } else if (check_result_value(j_result, G_ERROR_NOT_FOUND)) {
            response->status = 401;
          } else {
            y_log_message(Y_LOG_LEVEL_ERROR, "callback_glewlwyd_user_auth - Error get_users_for_session");
            response->status = 500;
          }
          o_free(session_uid);
          json_decref(j_result);
        }
      } else {
        if (json_string_length(json_object_get(j_param, "scheme_type")) && json_string_length(json_object_get(j_param, "scheme_name")) && json_is_object(json_object_get(j_param, "value"))) {
          j_result = auth_check_user_scheme(config, json_string_value(json_object_get(j_param, "scheme_type")), json_string_value(json_object_get(j_param, "scheme_name")), json_string_value(json_object_get(j_param, "username")), json_object_get(j_param, "value"), request);
          if (check_result_value(j_result, G_ERROR_PARAM)) {
            ulfius_set_string_body_response(response, 400, "bad scheme response");
          } else if (check_result_value(j_result, G_ERROR_UNAUTHORIZED)) {
            y_log_message(Y_LOG_LEVEL_WARNING, "Security - Authorization invalid for username %s at IP Address %s", json_string_value(json_object_get(j_param, "username")), ip_source);
            response->status = 401;
            glewlwyd_metrics_increment_counter_va(config, GLWD_METRICS_AUTH_USER_INVALID, 1, NULL);
            glewlwyd_metrics_increment_counter_va(config, GLWD_METRICS_AUTH_USER_INVALID_SCHEME, 1, "scheme_type", json_string_value(json_object_get(j_param, "scheme_type")), "scheme_name", json_string_value(json_object_get(j_param, "scheme_name")), NULL);
          } else if (check_result_value(j_result, G_ERROR_NOT_FOUND)) {
            response->status = 404;
          } else if (check_result_value(j_result, G_OK)) {
            if ((session_uid = get_session_id(config, request)) == NULL) {
              session_uid = generate_session_id();
            }
            if (user_session_update(config, session_uid, u_map_get_case(request->map_header, "user-agent"), issued_for, json_string_value(json_object_get(j_param, "username")), json_string_value(json_object_get(j_param, "scheme_name")), 1) != G_OK) {
              y_log_message(Y_LOG_LEVEL_ERROR, "callback_glewlwyd_user_auth - Error user_session_update (4)");
              response->status = 500;
            } else {
              ulfius_add_cookie_to_response(response, config->session_key, session_uid, expires, 0, config->cookie_domain, "/", config->cookie_secure, 0);
              y_log_message(Y_LOG_LEVEL_INFO, "Event - User '%s' authenticated with scheme '%s/%s'", json_string_value(json_object_get(j_param, "username")), json_string_value(json_object_get(j_param, "scheme_type")), json_string_value(json_object_get(j_param, "scheme_name")));
            }
            o_free(session_uid);
            glewlwyd_metrics_increment_counter_va(config, GLWD_METRICS_AUTH_USER_VALID, 1, NULL);
            glewlwyd_metrics_increment_counter_va(config, GLWD_METRICS_AUTH_USER_VALID_SCHEME, 1, "scheme_type", json_string_value(json_object_get(j_param, "scheme_type")), "scheme_name", json_string_value(json_object_get(j_param, "scheme_name")), NULL);
          } else {
            y_log_message(Y_LOG_LEVEL_ERROR, "callback_glewlwyd_user_auth - Error auth_check_user_scheme");
            response->status = 500;
          }
          json_decref(j_result);
        } else {
          ulfius_set_string_body_response(response, 400, "scheme_type, scheme_name and value are mandatory");
        }
      }
    } else {
      if (json_string_length(json_object_get(j_param, "scheme_type")) && json_string_length(json_object_get(j_param, "scheme_name")) && json_is_object(json_object_get(j_param, "value"))) {
        j_result = auth_check_identify_scheme(config, json_string_value(json_object_get(j_param, "scheme_type")), json_string_value(json_object_get(j_param, "scheme_name")), json_object_get(j_param, "value"), request);
        if (check_result_value(j_result, G_ERROR_PARAM)) {
          ulfius_set_string_body_response(response, 400, "bad scheme response");
        } else if (check_result_value(j_result, G_ERROR_UNAUTHORIZED)) {
          y_log_message(Y_LOG_LEVEL_WARNING, "Security - Authorization invalid for username <UNKNOWN> at IP Address %s", ip_source);
          response->status = 401;
        } else if (check_result_value(j_result, G_ERROR_NOT_FOUND)) {
          response->status = 404;
        } else if (check_result_value(j_result, G_OK)) {
          if ((session_uid = get_session_id(config, request)) == NULL) {
            session_uid = generate_session_id();
          }
          if (user_session_update(config, session_uid, u_map_get_case(request->map_header, "user-agent"), issued_for, json_string_value(json_object_get(j_result, "username")), json_string_value(json_object_get(j_param, "scheme_name")), 1) != G_OK) {
            y_log_message(Y_LOG_LEVEL_ERROR, "callback_glewlwyd_user_auth - Error user_session_update (4)");
            response->status = 500;
          } else {
            ulfius_add_cookie_to_response(response, config->session_key, session_uid, expires, 0, config->cookie_domain, "/", config->cookie_secure, 0);
            y_log_message(Y_LOG_LEVEL_INFO, "Event - User '%s' authenticated with scheme '%s/%s'", json_string_value(json_object_get(j_result, "username")), json_string_value(json_object_get(j_param, "scheme_type")), json_string_value(json_object_get(j_param, "scheme_name")));
          }
          o_free(session_uid);
        } else {
          y_log_message(Y_LOG_LEVEL_ERROR, "callback_glewlwyd_user_auth - Error auth_check_user_scheme");
          response->status = 500;
        }
        json_decref(j_result);
      } else {
        ulfius_set_string_body_response(response, 400, "username is mandatory");
      }
    }
  } else {
    ulfius_set_string_body_response(response, 400, "Input parameters must be in JSON format");
  }
  json_decref(j_param);
  o_free(issued_for);

  return U_CALLBACK_CONTINUE;
}