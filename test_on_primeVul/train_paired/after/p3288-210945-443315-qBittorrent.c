Http::Response AbstractWebApplication::processRequest(const Http::Request &request, const Http::Environment &env)
{
    session_ = 0;
    request_ = request;
    env_ = env;

    // clear response
    clear();

    // avoid clickjacking attacks
    header(Http::HEADER_X_FRAME_OPTIONS, "SAMEORIGIN");

    sessionInitialize();
    if (!sessionActive() && !isAuthNeeded())
        sessionStart();

    if (isBanned()) {
        status(403, "Forbidden");
        print(QObject::tr("Your IP address has been banned after too many failed authentication attempts."), Http::CONTENT_TYPE_TXT);
    }
    else {
        processRequest();
    }

    return response();
}