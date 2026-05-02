bool OpenConnectionTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (_dead) {
        _failed("Asked to die");
        return true;
    }
    using namespace Imap::Responses;

    if (model->accessParser(parser).connState == CONN_STATE_CONNECTED_PRETLS_PRECAPS) {
        if (!resp->tag.isEmpty()) {
            throw Imap::UnexpectedResponseReceived("Waiting for initial OK/BYE/PREAUTH, but got tagged response instead", *resp);
        }
    } else if (model->accessParser(parser).connState > CONN_STATE_CONNECTED_PRETLS_PRECAPS) {
        if (resp->tag.isEmpty()) {
            return false;
        }
    }

    switch (model->accessParser(parser).connState) {

    case CONN_STATE_AUTHENTICATED:
    case CONN_STATE_SELECTING:
    case CONN_STATE_SYNCING:
    case CONN_STATE_SELECTED:
    case CONN_STATE_FETCHING_PART:
    case CONN_STATE_FETCHING_MSG_METADATA:
    case CONN_STATE_LOGOUT:
    {
        QByteArray message = "No response expected by the OpenConnectionTask in state " +
                Imap::connectionStateToString(model->accessParser(parser).connState).toUtf8();
        // These shall not ever be reached by this code
        throw Imap::UnexpectedResponseReceived(message.constData(), *resp);
    }

    case CONN_STATE_NONE:
    case CONN_STATE_HOST_LOOKUP:
    case CONN_STATE_CONNECTING:
        // Looks like the corresponding stateChanged() signal could be delayed, at least with QProcess-based sockets
    case CONN_STATE_CONNECTED_PRETLS_PRECAPS:
        // We're connected now -- this is our initial state.
    {
        switch (resp->kind) {
        case PREAUTH:
            // Cool, we're already authenticated. Now, let's see if we have to issue CAPABILITY or if we already know that
            if (model->accessParser(parser).capabilitiesFresh) {
                // We're alsmost done here, apart from compression
                if (TROJITA_COMPRESS_DEFLATE && model->accessParser(parser).capabilities.contains(QLatin1String("COMPRESS=DEFLATE"))) {
                    compressCmd = parser->compressDeflate();
                    model->changeConnectionState(parser, CONN_STATE_COMPRESS_DEFLATE);
                } else {
                    // really done
                    model->changeConnectionState(parser, CONN_STATE_AUTHENTICATED);
                    onComplete();
                }
            } else {
                model->changeConnectionState(parser, CONN_STATE_POSTAUTH_PRECAPS);
                capabilityCmd = parser->capability();
            }
            return true;

        case OK:
            if (!model->accessParser(parser).capabilitiesFresh) {
                model->changeConnectionState(parser, CONN_STATE_CONNECTED_PRETLS);
                capabilityCmd = parser->capability();
            } else {
                startTlsOrLoginNow();
            }
            return true;

        case BYE:
            logout(tr("Server has closed the connection"));
            return true;

        case BAD:
            model->changeConnectionState(parser, CONN_STATE_LOGOUT);
            // If it was an ALERT, we've already warned the user
            if (resp->respCode != ALERT) {
                emit model->alertReceived(tr("The server replied with the following BAD response:\n%1").arg(resp->message));
            }
            logout(tr("Server has greeted us with a BAD response"));
            return true;

        default:
            throw Imap::UnexpectedResponseReceived("Waiting for initial OK/BYE/BAD/PREAUTH, but got this instead", *resp);
        }
        break;
    }

    case CONN_STATE_CONNECTED_PRETLS:
        // We've asked for capabilities upon the initial interaction
    {
        bool wasCaps = checkCapabilitiesResult(resp);
        if (wasCaps && !_finished) {
            startTlsOrLoginNow();
        }
        return wasCaps;
    }

    case CONN_STATE_STARTTLS_ISSUED:
    {
        if (resp->tag == startTlsCmd) {
            if (resp->kind == OK) {
                model->changeConnectionState(parser, CONN_STATE_STARTTLS_HANDSHAKE);
                if (!model->m_startTls) {
                    // The model was not configured to perform STARTTLS, but we still did that for some reason.
                    // As suggested by Mike Cardwell on the trojita ML (http://article.gmane.org/gmane.mail.trojita.general/299),
                    // it makes sense to make this settings permanent, so that a user is not tricked into revealing their
                    // password when a MITM removes the LOGINDISABLED in future.
                    EMIT_LATER_NOARG(model, requireStartTlsInFuture);
                }
            } else {
                logout(tr("STARTTLS failed: %1").arg(resp->message));
            }
            return true;
        }
        return false;
    }

    case CONN_STATE_SSL_HANDSHAKE:
    case CONN_STATE_STARTTLS_HANDSHAKE:
        // nothing should really arrive at this point; the Parser is expected to wait for encryption and only after that
        // send the data
        Q_ASSERT(false);
        return false;

    case CONN_STATE_STARTTLS_VERIFYING:
    case CONN_STATE_SSL_VERIFYING:
    {
        // We're waiting for a decision based on a policy, so we do not really expect any network IO at this point
        // FIXME: an assert(false) here?
        qDebug() << "OpenConnectionTask: ignoring response, we're still waiting for SSL policy decision";
        return false;
    }

    case CONN_STATE_ESTABLISHED_PRECAPS:
        // Connection is established and we're waiting for updated capabilities
    {
        bool wasCaps = checkCapabilitiesResult(resp);
        if (wasCaps && !_finished) {
            if (model->accessParser(parser).capabilities.contains(QLatin1String("LOGINDISABLED"))) {
                logout(tr("Capabilities still contain LOGINDISABLED even after STARTTLS"));
            } else {
                model->changeConnectionState(parser, CONN_STATE_LOGIN);
                askForAuth();
            }
        }
        return wasCaps;
    }

    case CONN_STATE_LOGIN:
        // Check the result of the LOGIN command
    {
        if (resp->tag == loginCmd) {
            loginCmd.clear();
            // The LOGIN command is finished
            if (resp->kind == OK) {
                if (resp->respCode == CAPABILITIES || model->accessParser(parser).capabilitiesFresh) {
                    // Capabilities are already known
                    if (TROJITA_COMPRESS_DEFLATE && model->accessParser(parser).capabilities.contains(QLatin1String("COMPRESS=DEFLATE"))) {
                        compressCmd = parser->compressDeflate();
                        model->changeConnectionState(parser, CONN_STATE_COMPRESS_DEFLATE);
                    } else {
                        model->changeConnectionState(parser, CONN_STATE_AUTHENTICATED);
                        onComplete();
                    }
                } else {
                    // Got to ask for the capabilities
                    model->changeConnectionState(parser, CONN_STATE_POSTAUTH_PRECAPS);
                    capabilityCmd = parser->capability();
                }
            } else {
                // Login failed
                QString message;
                switch (resp->respCode) {
                case Responses::UNAVAILABLE:
                    message = tr("Temporary failure because a subsystem is down.");
                    break;
                case Responses::AUTHENTICATIONFAILED:
                    message = tr("Authentication failed.  This often happens due to bad password or wrong user name.");
                    break;
                case Responses::AUTHORIZATIONFAILED:
                    message = tr("Authentication succeeded in using the authentication identity, "
                                 "but the server cannot or will not allow the authentication "
                                 "identity to act as the requested authorization identity.");
                    break;
                case Responses::EXPIRED:
                    message = tr("Either authentication succeeded or the server no longer had the "
                                 "necessary data; either way, access is no longer permitted using "
                                 "that passphrase.  You should get a new passphrase.");
                    break;
                case Responses::PRIVACYREQUIRED:
                    message = tr("The operation is not permitted due to a lack of privacy.");
                    break;
                case Responses::CONTACTADMIN:
                    message = tr("You should contact the system administrator or support desk.");
                    break;
                default:
                    break;
                }

                if (message.isEmpty()) {
                    message = tr("Login failed: %1").arg(resp->message);
                } else {
                    message = tr("%1\n\n%2").arg(message, resp->message);
                }
                EMIT_LATER(model, authAttemptFailed, Q_ARG(QString, message));
                model->m_imapPassword.clear();
                model->m_hasImapPassword = false;
                if (model->accessParser(parser).connState == CONN_STATE_LOGOUT) {
                    // The server has closed the conenction
                    _failed(QLatin1String("Connection closed after a failed login"));
                    return true;
                }
                askForAuth();
            }
            return true;
        }
        return false;
    }

    case CONN_STATE_POSTAUTH_PRECAPS:
    {
        bool wasCaps = checkCapabilitiesResult(resp);
        if (wasCaps && !_finished) {
            model->changeConnectionState(parser, CONN_STATE_AUTHENTICATED);
            onComplete();
        }
        return wasCaps;
    }

    case CONN_STATE_COMPRESS_DEFLATE:
        if (resp->tag == compressCmd) {
            model->changeConnectionState(parser, CONN_STATE_AUTHENTICATED);
            onComplete();
            return true;
        } else {
            return false;
        }
        break;

    }

    // Required catch-all for OpenSuSE's build service (Tumbleweed, 2012-04-03)
    Q_ASSERT(false);
    return false;
}