    void Display::startAuth(const QString &user, const QString &password, const Session &session) {
        m_passPhrase = password;

        // sanity check
        if (!session.isValid()) {
            qCritical() << "Invalid session" << session.fileName();
            return;
        }
        if (session.xdgSessionType().isEmpty()) {
            qCritical() << "Failed to find XDG session type for session" << session.fileName();
            return;
        }
        if (session.exec().isEmpty()) {
            qCritical() << "Failed to find command for session" << session.fileName();
            return;
        }

        QString existingSessionId;

        if (Logind::isAvailable() && mainConfig.Users.ReuseSession.get()) {
            OrgFreedesktopLogin1ManagerInterface manager(Logind::serviceName(), Logind::managerPath(), QDBusConnection::systemBus());
            auto reply = manager.ListSessions();
            reply.waitForFinished();

            foreach(const SessionInfo &s, reply.value()) {
                if (s.userName == user) {
                    OrgFreedesktopLogin1SessionInterface session(Logind::serviceName(), s.sessionPath.path(), QDBusConnection::systemBus());
                    if (session.service() == QLatin1String("sddm")) {
                        existingSessionId =  s.sessionId;
                        break;
                    }
                }
            }
        }

        // cache last session
        m_lastSession = session;

        // save session desktop file name, we'll use it to set the
        // last session later, in slotAuthenticationFinished()
        m_sessionName = session.fileName();

        // some information
        qDebug() << "Session" << m_sessionName << "selected, command:" << session.exec();

        // create new VT for Wayland sessions otherwise use greeter vt
        int vt = terminalId();
        if (session.xdgSessionType() == QLatin1String("wayland"))
            vt = VirtualTerminal::setUpNewVt();
        m_lastSession.setVt(vt);

        QProcessEnvironment env;
        env.insert(QStringLiteral("PATH"), mainConfig.Users.DefaultPath.get());
        if (session.xdgSessionType() == QLatin1String("x11"))
            env.insert(QStringLiteral("DISPLAY"), name());
        env.insert(QStringLiteral("XDG_SEAT_PATH"), daemonApp->displayManager()->seatPath(seat()->name()));
        env.insert(QStringLiteral("XDG_SESSION_PATH"), daemonApp->displayManager()->sessionPath(QStringLiteral("Session%1").arg(daemonApp->newSessionId())));
        env.insert(QStringLiteral("DESKTOP_SESSION"), session.desktopSession());
        env.insert(QStringLiteral("XDG_CURRENT_DESKTOP"), session.desktopNames());
        env.insert(QStringLiteral("XDG_SESSION_CLASS"), QStringLiteral("user"));
        env.insert(QStringLiteral("XDG_SESSION_TYPE"), session.xdgSessionType());
        env.insert(QStringLiteral("XDG_SEAT"), seat()->name());

        env.insert(QStringLiteral("XDG_SESSION_DESKTOP"), session.desktopNames());
        if (seat()->name() == QLatin1String("seat0")) {
            env.insert(QStringLiteral("XDG_VTNR"), QString::number(vt));
        }

        m_auth->insertEnvironment(env);

        m_auth->setUser(user);
        if (existingSessionId.isNull()) {
            m_auth->setSession(session.exec());
        } else {
            //we only want to unlock the session if we can lock in, so we want to go via PAM auth, but not start a new session
            //by not setting the session and the helper will emit authentication and then quit
            connect(m_auth, &Auth::authentication, this, [=](){
                qDebug() << "activating existing seat";
                OrgFreedesktopLogin1ManagerInterface manager(Logind::serviceName(), Logind::managerPath(), QDBusConnection::systemBus());
                manager.UnlockSession(existingSessionId);
                manager.ActivateSession(existingSessionId);
            });
        }
        m_auth->start();
    }