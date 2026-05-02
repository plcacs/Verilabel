static size_t handle_returned_header (void *ptr, size_t size, size_t nmemb, void *stream)
{
    auth_client *auth_user = stream;
    size_t len = size * nmemb;
    client_t *client = auth_user->client;

    if (client) {
        auth_t *auth = client->auth;
        auth_url *url = auth->state;

        if (url->auth_header && len >= url->auth_header_len && strncasecmp(ptr, url->auth_header, url->auth_header_len) == 0)
            client->authenticated = 1;

        if (url->timelimit_header && len > url->timelimit_header_len && strncasecmp(ptr, url->timelimit_header, url->timelimit_header_len) == 0) {
            const char *input = ptr;
            unsigned int limit = 0;

            if (len >= 2 && input[len - 2] == '\r' && input[len - 1] == '\n') {
                input += url->timelimit_header_len;

                if (sscanf(input, "%u\r\n", &limit) == 1) {
                    client->con->discon_time = time(NULL) + limit;
                } else {
                    ICECAST_LOG_ERROR("Auth backend returned invalid timeline header: Can not parse limit");
                }
            } else {
                ICECAST_LOG_ERROR("Auth backend returned invalid timelimit header.");
            }
        }

        if (len > 24 && strncasecmp(ptr, "icecast-auth-message: ", 22) == 0) {
            const char *input = ptr;
            size_t copy_len = len - 24 + 1; /* length of string plus \0-termination */

            if (copy_len > sizeof(url->errormsg)) {
                copy_len = sizeof(url->errormsg);
            }

            if (len >= 2 && input[len - 2] == '\r' && input[len - 1] == '\n') {
                input += 22;
                memcpy(url->errormsg, input, copy_len);
                url->errormsg[copy_len-1] = 0;
            } else {
                ICECAST_LOG_ERROR("Auth backend returned invalid message header.");
            }
        }
    }

    return len;
}