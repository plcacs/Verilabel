urnParseReply(const char *inbuf, const HttpRequestMethod& m)
{
    char *buf = xstrdup(inbuf);
    char *token;
    url_entry *list;
    url_entry *old;
    int n = 32;
    int i = 0;
    debugs(52, 3, "urnParseReply");
    list = (url_entry *)xcalloc(n + 1, sizeof(*list));

    for (token = strtok(buf, crlf); token; token = strtok(NULL, crlf)) {
        debugs(52, 3, "urnParseReply: got '" << token << "'");

        if (i == n) {
            old = list;
            n <<= 2;
            list = (url_entry *)xcalloc(n + 1, sizeof(*list));
            memcpy(list, old, i * sizeof(*list));
            safe_free(old);
        }

        AnyP::Uri uri;
        if (!uri.parse(m, SBuf(token)) || !*uri.host())
            continue;

#if USE_ICMP
        list[i].rtt = netdbHostRtt(uri.host());

        if (0 == list[i].rtt) {
            debugs(52, 3, "Pinging " << uri.host());
            netdbPingSite(uri.host());
        }
#else
        list[i].rtt = 0;
#endif

        list[i].url = xstrdup(uri.absolute().c_str());
        list[i].host = xstrdup(uri.host());
        // TODO: Use storeHas() or lock/unlock entry to avoid creating unlocked
        // ones.
        list[i].flags.cached = storeGetPublic(list[i].url, m) ? 1 : 0;
        ++i;
    }

    debugs(52, 3, "urnParseReply: Found " << i << " URLs");
    return list;
}