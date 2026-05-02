	virtual EModRet OnPrivCTCP(CNick& Nick, CString& sMessage) {
		if (sMessage.Equals("DCC ", false, 4) && m_pUser->IsUserAttached()) {
			// DCC CHAT chat 2453612361 44592
			CString sType = sMessage.Token(1);
			CString sFile = sMessage.Token(2);
			unsigned long uLongIP = sMessage.Token(3).ToULong();
			unsigned short uPort = sMessage.Token(4).ToUShort();
			unsigned long uFileSize = sMessage.Token(5).ToULong();

			if (sType.Equals("CHAT")) {
				CNick FromNick(Nick.GetNickMask());
				unsigned short uBNCPort = CDCCBounce::DCCRequest(FromNick.GetNick(), uLongIP, uPort, "", true, this, CUtils::GetIP(uLongIP));
				if (uBNCPort) {
					CString sIP = GetLocalDCCIP();
					m_pUser->PutUser(":" + Nick.GetNickMask() + " PRIVMSG " + m_pUser->GetNick() + " :\001DCC CHAT chat " + CString(CUtils::GetLongIP(sIP)) + " " + CString(uBNCPort) + "\001");
				}
			} else if (sType.Equals("SEND")) {
				// DCC SEND readme.txt 403120438 5550 1104
				unsigned short uBNCPort = CDCCBounce::DCCRequest(Nick.GetNick(), uLongIP, uPort, sFile, false, this, CUtils::GetIP(uLongIP));
				if (uBNCPort) {
					CString sIP = GetLocalDCCIP();
					m_pUser->PutUser(":" + Nick.GetNickMask() + " PRIVMSG " + m_pUser->GetNick() + " :\001DCC SEND " + sFile + " " + CString(CUtils::GetLongIP(sIP)) + " " + CString(uBNCPort) + " " + CString(uFileSize) + "\001");
				}
			} else if (sType.Equals("RESUME")) {
				// Need to lookup the connection by port, filter the port, and forward to the user
				unsigned short uResumePort = sMessage.Token(3).ToUShort();

				set<CSocket*>::const_iterator it;
				for (it = BeginSockets(); it != EndSockets(); ++it) {
					CDCCBounce* pSock = (CDCCBounce*) *it;

					if (pSock->GetLocalPort() == uResumePort) {
						m_pUser->PutUser(":" + Nick.GetNickMask() + " PRIVMSG " + m_pUser->GetNick() + " :\001DCC " + sType + " " + sFile + " " + CString(pSock->GetUserPort()) + " " + sMessage.Token(4) + "\001");
					}
				}
			} else if (sType.Equals("ACCEPT")) {
				// Need to lookup the connection by port, filter the port, and forward to the user
				set<CSocket*>::const_iterator it;
				for (it = BeginSockets(); it != EndSockets(); ++it) {
					CDCCBounce* pSock = (CDCCBounce*) *it;

					if (pSock->GetUserPort() == sMessage.Token(3).ToUShort()) {
						m_pUser->PutUser(":" + Nick.GetNickMask() + " PRIVMSG " + m_pUser->GetNick() + " :\001DCC " + sType + " " + sFile + " " + CString(pSock->GetLocalPort()) + " " + sMessage.Token(4) + "\001");
					}
				}
			}

			return HALTCORE;
		}

		return CONTINUE;
	}