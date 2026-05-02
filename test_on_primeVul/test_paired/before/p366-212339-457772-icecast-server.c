static size_t handle_returned_header (void *ptr, size_t size, size_t nmemb, void *stream)
{
    auth_client *auth_user = stream;
    size_t bytes = size * nmemb;
    client_t *client = auth_user->client;

    if (client)
    {
        auth_t *auth = client->auth;
        auth_url *url = auth->state;
        if (strncasecmp (ptr, url->auth_header, url->auth_header_len) == 0)
            client->authenticated = 1;
        if (strncasecmp (ptr, url->timelimit_header, url->timelimit_header_len) == 0)
        {
            unsigned int limit = 0;
            sscanf ((char *)ptr+url->timelimit_header_len, "%u\r\n", &limit);
            client->con->discon_time = time(NULL) + limit;
        }
        if (strncasecmp (ptr, "icecast-auth-message: ", 22) == 0)
        {
            char *eol;
            snprintf (url->errormsg, sizeof (url->errormsg), "%s", (char*)ptr+22);
            eol = strchr (url->errormsg, '\r');
            if (eol == NULL)
                eol = strchr (url->errormsg, '\n');
            if (eol)
                *eol = '\0';
        }
    }

    return bytes;
}