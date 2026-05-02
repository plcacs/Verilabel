    Status CmdAuthenticate::_authenticateX509(const UserName& user, const BSONObj& cmdObj) {
        if (!getSSLManager()) {
            return Status(ErrorCodes::ProtocolError,
                          "SSL support is required for the MONGODB-X509 mechanism.");
        }
        if(user.getDB() != "$external") {
            return Status(ErrorCodes::ProtocolError,
                          "X.509 authentication must always use the $external database.");
        }

        ClientBasic *client = ClientBasic::getCurrent();
        AuthorizationSession* authorizationSession = client->getAuthorizationSession();
        std::string subjectName = client->port()->getX509SubjectName();

        if (user.getUser() != subjectName) {
            return Status(ErrorCodes::AuthenticationFailed,
                          "There is no x.509 client certificate matching the user.");
        }
        else {
            std::string srvSubjectName = getSSLManager()->getServerSubjectName();
            std::string srvClusterId = srvSubjectName.substr(srvSubjectName.find(",OU="));
            std::string peerClusterId = subjectName.substr(subjectName.find(",OU="));

            fassert(17002, !srvClusterId.empty() && srvClusterId != srvSubjectName);

            // Handle internal cluster member auth, only applies to server-server connections
            int clusterAuthMode = serverGlobalParams.clusterAuthMode.load(); 
            if (srvClusterId == peerClusterId) {
                if (clusterAuthMode == ServerGlobalParams::ClusterAuthMode_undefined ||
                    clusterAuthMode == ServerGlobalParams::ClusterAuthMode_keyFile) {
                    return Status(ErrorCodes::AuthenticationFailed, "The provided certificate " 
                                  "can only be used for cluster authentication, not client " 
                                  "authentication. The current configuration does not allow " 
                                  "x.509 cluster authentication, check the --clusterAuthMode flag");
                }
                authorizationSession->grantInternalAuthorization();
            }
            // Handle normal client authentication, only applies to client-server connections
            else {
                if (_isX509AuthDisabled) {
                    return Status(ErrorCodes::BadValue,
                                  _x509AuthenticationDisabledMessage);
                }
                Status status = authorizationSession->addAndAuthorizeUser(user);
                if (!status.isOK()) {
                    return status;
                }
            }
            return Status::OK();
        }
    }