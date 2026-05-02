vncProperties::DialogProc(HWND hwnd,
						  UINT uMsg,
						  WPARAM wParam,
						  LPARAM lParam )
{
	// We use the dialog-box's USERDATA to store a _this pointer
	// This is set only once WM_INITDIALOG has been recieved, though!
     vncProperties *_this = helper::SafeGetWindowUserData<vncProperties>(hwnd);

	switch (uMsg)
	{

	case WM_INITDIALOG:
		{			
			vnclog.Print(LL_INTINFO, VNCLOG("INITDIALOG properties\n"));
			// Retrieve the Dialog box parameter and use it as a pointer
			// to the calling vncProperties object
            helper::SafeSetWindowUserData(hwnd, lParam);

			_this = (vncProperties *) lParam;
			_this->m_dlgvisible = TRUE;
			if (_this->m_fUseRegistry)
			{
				_this->Load(_this->m_usersettings);

				// Set the dialog box's title to indicate which Properties we're editting
				if (_this->m_usersettings) {
					SetWindowText(hwnd, sz_ID_CURRENT_USER_PROP);
				} else {
					SetWindowText(hwnd, sz_ID_DEFAULT_SYST_PROP);
				}
			}
			else
			{
				_this->LoadFromIniFile();
			}

			// Initialise the properties controls
			HWND hConnectSock = GetDlgItem(hwnd, IDC_CONNECT_SOCK);

			// Tight 1.2.7 method
			BOOL bConnectSock = _this->m_server->SockConnected();
			SendMessage(hConnectSock, BM_SETCHECK, bConnectSock, 0);

			// Set the content of the password field to a predefined string.
		    SetDlgItemText(hwnd, IDC_PASSWORD, "~~~~~~~~");
			EnableWindow(GetDlgItem(hwnd, IDC_PASSWORD), bConnectSock);

			// Set the content of the view-only password field to a predefined string. //PGM
		    SetDlgItemText(hwnd, IDC_PASSWORD2, "~~~~~~~~"); //PGM
			EnableWindow(GetDlgItem(hwnd, IDC_PASSWORD2), bConnectSock); //PGM

			// Set the initial keyboard focus
			if (bConnectSock)
			{
				SetFocus(GetDlgItem(hwnd, IDC_PASSWORD));
				SendDlgItemMessage(hwnd, IDC_PASSWORD, EM_SETSEL, 0, (LPARAM)-1);
			}
			else
				SetFocus(hConnectSock);
			// Set display/ports settings
			_this->InitPortSettings(hwnd);

			HWND hConnectHTTP = GetDlgItem(hwnd, IDC_CONNECT_HTTP);
			SendMessage(hConnectHTTP,
				BM_SETCHECK,
				_this->m_server->HTTPConnectEnabled(),
				0);

		   // Modif sf@2002 - v1.1.0
		   HWND hFileTransfer = GetDlgItem(hwnd, IDC_FILETRANSFER);
           SendMessage(hFileTransfer, BM_SETCHECK, _this->m_server->FileTransferEnabled(), 0);

		   HWND hFileTransferUserImp = GetDlgItem(hwnd, IDC_FTUSERIMPERSONATION_CHECK);
           SendMessage(hFileTransferUserImp, BM_SETCHECK, _this->m_server->FTUserImpersonation(), 0);
		   
		   HWND hBlank = GetDlgItem(hwnd, IDC_BLANK);
           SendMessage(hBlank, BM_SETCHECK, _this->m_server->BlankMonitorEnabled(), 0);
		   if (!VNC_OSVersion::getInstance()->OS_WIN10_TRANS && VNC_OSVersion::getInstance()->OS_WIN10)
			   SetDlgItemText(hwnd, IDC_BLANK, "Enable Blank Monitor on Viewer Request require Min Win10 build 19041 ");
		   if (VNC_OSVersion::getInstance()->OS_WIN8)
			   SetDlgItemText(hwnd, IDC_BLANK, "Enable Blank Monitor on Viewer Not supported on windows 8 ");
		   HWND hBlank2 = GetDlgItem(hwnd, IDC_BLANK2); //PGM
           SendMessage(hBlank2, BM_SETCHECK, _this->m_server->BlankInputsOnly(), 0); //PGM
		   
		   HWND hLoopback = GetDlgItem(hwnd, IDC_ALLOWLOOPBACK);
		   BOOL fLoopback = _this->m_server->LoopbackOk();
		   SendMessage(hLoopback, BM_SETCHECK, fLoopback, 0);
#ifdef IPV6V4
		   HWND hIPV6 = GetDlgItem(hwnd, IDC_IPV6);
		   BOOL fIPV6 = _this->m_server->IPV6();
		   SendMessage(hIPV6, BM_SETCHECK, fIPV6, 0);
#else
		   HWND hIPV6 = GetDlgItem(hwnd, IDC_IPV6);
		   EnableWindow(hIPV6, false);
#endif
		   HWND hLoopbackonly = GetDlgItem(hwnd, IDC_LOOPBACKONLY);
		   BOOL fLoopbackonly = _this->m_server->LoopbackOnly();
		   SendMessage(hLoopbackonly, BM_SETCHECK, fLoopbackonly, 0);

		   HWND hTrayicon = GetDlgItem(hwnd, IDC_DISABLETRAY);
		   BOOL fTrayicon = _this->m_server->GetDisableTrayIcon();
		   SendMessage(hTrayicon, BM_SETCHECK, fTrayicon, 0);

		   HWND hrdpmode = GetDlgItem(hwnd, IDC_RDPMODE);
		   BOOL frdpmode = _this->m_server->GetRdpmode();
		   SendMessage(hrdpmode, BM_SETCHECK, frdpmode, 0);

		   HWND hNoScreensaver= GetDlgItem(hwnd,IDC_NOSCREENSAVER);
		   BOOL fNoScrensaver = _this->m_server->GetNoScreensaver();
		   SendMessage(hNoScreensaver, BM_SETCHECK, fNoScrensaver, 0);

		   HWND hAllowshutdown = GetDlgItem(hwnd, IDC_ALLOWSHUTDOWN);
		   SendMessage(hAllowshutdown, BM_SETCHECK, !_this->m_allowshutdown , 0);

		   HWND hm_alloweditclients = GetDlgItem(hwnd, IDC_ALLOWEDITCLIENTS);
		   SendMessage(hm_alloweditclients, BM_SETCHECK, !_this->m_alloweditclients , 0);
		   _this->m_server->SetAllowEditClients(_this->m_alloweditclients);
		   

		   if (vnclog.GetMode() >= 2)
			   CheckDlgButton(hwnd, IDC_LOG, BST_CHECKED);
		   else
			   CheckDlgButton(hwnd, IDC_LOG, BST_UNCHECKED);

#ifndef AVILOG
		   ShowWindow (GetDlgItem(hwnd, IDC_CLEAR), SW_HIDE);
		   ShowWindow (GetDlgItem(hwnd, IDC_VIDEO), SW_HIDE);
#endif
		   if (vnclog.GetVideo())
		   {
			   SetDlgItemText(hwnd, IDC_EDIT_PATH, vnclog.GetPath());
			   //EnableWindow(GetDlgItem(hwnd, IDC_EDIT_PATH), true);
			   CheckDlgButton(hwnd, IDC_VIDEO, BST_CHECKED);
		   }
		   else
		   {
			   SetDlgItemText(hwnd, IDC_EDIT_PATH, vnclog.GetPath());
			   //EnableWindow(GetDlgItem(hwnd, IDC_EDIT_PATH), false);
			   CheckDlgButton(hwnd, IDC_VIDEO, BST_UNCHECKED);
		   }
		   
			// Marscha@2004 - authSSP: moved MS-Logon checkbox back to admin props page
			// added New MS-Logon checkbox
			// only enable New MS-Logon checkbox and Configure MS-Logon groups when MS-Logon
			// is checked.
		   HWND hSecure = GetDlgItem(hwnd, IDC_SAVEPASSWORDSECURE);
		   SendMessage(hSecure, BM_SETCHECK, _this->m_server->Secure(), 0);
		   
			HWND hMSLogon = GetDlgItem(hwnd, IDC_MSLOGON_CHECKD);
			SendMessage(hMSLogon, BM_SETCHECK, _this->m_server->MSLogonRequired(), 0);

			HWND hNewMSLogon = GetDlgItem(hwnd, IDC_NEW_MSLOGON);
			SendMessage(hNewMSLogon, BM_SETCHECK, _this->m_server->GetNewMSLogon(), 0);

			HWND hReverseAuth = GetDlgItem(hwnd, IDC_REVERSEAUTH);
			SendMessage(hReverseAuth, BM_SETCHECK, _this->m_server->GetReverseAuthRequired(), 0);

			EnableWindow(GetDlgItem(hwnd, IDC_NEW_MSLOGON), _this->m_server->MSLogonRequired());
			EnableWindow(GetDlgItem(hwnd, IDC_MSLOGON), _this->m_server->MSLogonRequired());
			// Marscha@2004 - authSSP: end of change

		   SetDlgItemInt(hwnd, IDC_SCALE, _this->m_server->GetDefaultScale(), false);

		   
		   // Remote input settings
			HWND hEnableRemoteInputs = GetDlgItem(hwnd, IDC_DISABLE_INPUTS);
			SendMessage(hEnableRemoteInputs,
				BM_SETCHECK,
				!(_this->m_server->RemoteInputsEnabled()),
				0);

			// Local input settings
			HWND hDisableLocalInputs = GetDlgItem(hwnd, IDC_DISABLE_LOCAL_INPUTS);
			SendMessage(hDisableLocalInputs,
				BM_SETCHECK,
				_this->m_server->LocalInputsDisabled(),
				0);

			// japanese keybaord
			HWND hJapInputs = GetDlgItem(hwnd, IDC_JAP_INPUTS);
			SendMessage(hJapInputs,
				BM_SETCHECK,
				_this->m_server->JapInputEnabled(),
				0);

			HWND hUnicodeInputs = GetDlgItem(hwnd, IDC_UNICODE_INPUTS);
			SendMessage(hUnicodeInputs,
				BM_SETCHECK,
				_this->m_server->UnicodeInputEnabled(),
				0);

			HWND hwinhelper = GetDlgItem(hwnd, IDC_WIN8_HELPER);
			SendMessage(hwinhelper,
				BM_SETCHECK,
				_this->m_server->Win8HelperEnabled(),
				0);

			// Remove the wallpaper
			HWND hRemoveWallpaper = GetDlgItem(hwnd, IDC_REMOVE_WALLPAPER);
			SendMessage(hRemoveWallpaper,
				BM_SETCHECK,
				_this->m_server->RemoveWallpaperEnabled(),
				0);

			// Lock settings
			HWND hLockSetting;
			switch (_this->m_server->LockSettings()) {
			case 1:
				hLockSetting = GetDlgItem(hwnd, IDC_LOCKSETTING_LOCK);
				break;
			case 2:
				hLockSetting = GetDlgItem(hwnd, IDC_LOCKSETTING_LOGOFF);
				break;
			default:
				hLockSetting = GetDlgItem(hwnd, IDC_LOCKSETTING_NOTHING);
			};
			SendMessage(hLockSetting,
				BM_SETCHECK,
				TRUE,
				0);

			HWND hNotificationSelection;
			switch (_this->m_server->getNotificationSelection()) {
			case 1:
				hNotificationSelection = GetDlgItem(hwnd, IDC_RADIONOTIFICATIONIFPROVIDED);
				break;
			default:
				hNotificationSelection = GetDlgItem(hwnd,
					IDC_RADIONOTIFICATIONON);
				break;
			};
			SendMessage(hNotificationSelection,
				BM_SETCHECK,
				TRUE,
				0);

			HWND hmvSetting = 0;
			switch (_this->m_server->ConnectPriority()) {
			case 0:
				hmvSetting = GetDlgItem(hwnd, IDC_MV1);
				break;
			case 1:
				hmvSetting = GetDlgItem(hwnd, IDC_MV2);
				break;
			case 2:
				hmvSetting = GetDlgItem(hwnd, IDC_MV3);
				break;
			case 3:
				hmvSetting = GetDlgItem(hwnd, IDC_MV4);
				break;
			};
			SendMessage(hmvSetting,
				BM_SETCHECK,
				TRUE,
				0);


			HWND hQuerySetting;
			switch (_this->m_server->QueryAccept()) {
			case 0:
				hQuerySetting = GetDlgItem(hwnd, IDC_DREFUSE);
				break;
			case 1:
				hQuerySetting = GetDlgItem(hwnd, IDC_DACCEPT);
				break;
			default:
				hQuerySetting = GetDlgItem(hwnd, IDC_DREFUSE);
			};
			SendMessage(hQuerySetting,
				BM_SETCHECK,
				TRUE,
				0);

			HWND hMaxViewerSetting;
			switch (_this->m_server->getMaxViewerSetting()) {
			case 0:
				hMaxViewerSetting = GetDlgItem(hwnd, IDC_MAXREFUSE);
				break;
			case 1:
				hMaxViewerSetting = GetDlgItem(hwnd, IDC_MAXDISCONNECT);
				break;
			default:
				hMaxViewerSetting = GetDlgItem(hwnd, IDC_MAXREFUSE);
			};
			SendMessage(hMaxViewerSetting,
				BM_SETCHECK,
				TRUE,
				0);

			HWND hCollabo = GetDlgItem(hwnd, IDC_COLLABO);
			SendMessage(hCollabo,
				BM_SETCHECK,
				_this->m_server->getCollabo(),
				0);

			HWND hwndDlg = GetDlgItem(hwnd, IDC_FRAME);
			SendMessage(hwndDlg,
				BM_SETCHECK,
				_this->m_server->getFrame(),
				0);

			hwndDlg = GetDlgItem(hwnd, IDC_NOTIFOCATION);
			SendMessage(hwndDlg,
				BM_SETCHECK,
				_this->m_server->getNotification(),
				0);

			hwndDlg = GetDlgItem(hwnd, IDC_OSD);
			SendMessage(hwndDlg,
				BM_SETCHECK,
				_this->m_server->getOSD(),
				0);

			char maxviewersChar[128];
			UINT maxviewers = _this->m_server->getMaxViewers();
			sprintf_s(maxviewersChar, "%d", (int)maxviewers);
			SetDlgItemText(hwnd, IDC_MAXVIEWERS, (const char*)maxviewersChar);

			// sf@2002 - List available DSM Plugins
			HWND hPlugins = GetDlgItem(hwnd, IDC_PLUGINS_COMBO);
			int nPlugins = _this->m_server->GetDSMPluginPointer()->ListPlugins(hPlugins);
			if (!nPlugins) 
			{
				SendMessage(hPlugins, CB_ADDSTRING, 0, (LPARAM) sz_ID_NO_PLUGIN_DETECT);
				SendMessage(hPlugins, CB_SETCURSEL, 0, 0);
			}
			else
				SendMessage(hPlugins, CB_SELECTSTRING, 0, (LPARAM)_this->m_server->GetDSMPluginName());

			// Modif sf@2002
			SendMessage(GetDlgItem(hwnd, IDC_PLUGIN_CHECK), BM_SETCHECK, _this->m_server->IsDSMPluginEnabled(), 0);
			EnableWindow(GetDlgItem(hwnd, IDC_PLUGIN_BUTTON), _this->m_server->IsDSMPluginEnabled());

			// Query window option - Taken from TightVNC advanced properties 
			BOOL queryEnabled = (_this->m_server->QuerySetting() == 4);
			SendMessage(GetDlgItem(hwnd, IDQUERY), BM_SETCHECK, queryEnabled, 0);
			EnableWindow(GetDlgItem(hwnd, IDQUERYTIMEOUT), queryEnabled);
			EnableWindow(GetDlgItem(hwnd, IDC_QUERYDISABLETIME), queryEnabled);
			EnableWindow(GetDlgItem(hwnd, IDC_DREFUSE), queryEnabled);
			EnableWindow(GetDlgItem(hwnd, IDC_DACCEPT), queryEnabled);

			SetDlgItemText(hwnd, IDC_SERVICE_COMMANDLINE, _this->service_commandline);
			SetDlgItemText(hwnd, IDC_EDITQUERYTEXT, _this->accept_reject_mesg);


			char timeout[128];
			UINT t = _this->m_server->QueryTimeout();
			sprintf_s(timeout, "%d", (int)t);
		    SetDlgItemText(hwnd, IDQUERYTIMEOUT, (const char *) timeout);

			char disableTime[128];
			UINT tt = _this->m_server->QueryDisableTime();
			sprintf_s(disableTime, "%d", (int)tt);
		    SetDlgItemText(hwnd, IDC_QUERYDISABLETIME, (const char *) disableTime);

			_this->ExpandBox(hwnd, !_this->m_bExpanded);
			SendMessage(GetDlgItem(hwnd, IDC_BUTTON_EXPAND), BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)_this->hBmpExpand);

			SetForegroundWindow(hwnd);

			return FALSE; // Because we've set the focus
		}

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_SHOWOPTIONS:
		case IDC_BUTTON_EXPAND:
			_this->ExpandBox(hwnd, !_this->m_bExpanded);
//			if (_this->m_bExpanded)
//				_this->InitTab(hwnd);
			return TRUE;
		case IDOK:
		case IDC_APPLY:
			{
				char path[512];
				int lenpath = GetDlgItemText(hwnd, IDC_EDIT_PATH, (LPSTR) &path, 512);
				if (lenpath != 0)
					{
						vnclog.SetPath(path);
				}
				else
				{
					strcpy_s(path,"");
					vnclog.SetPath(path);
				}
				bool Secure_old = _this->m_server->Secure();
				HWND hSecure = GetDlgItem(hwnd, IDC_SAVEPASSWORDSECURE);
				_this->m_server->Secure(SendMessage(hSecure, BM_GETCHECK, 0, 0) == BST_CHECKED);

				// Save the password
				char passwd[MAXPWLEN+1];
				char passwd2[MAXPWLEN+1];
				memset(passwd, '\0', MAXPWLEN + 1); //PGM
				memset(passwd2, '\0', MAXPWLEN + 1); //PGM
				// TightVNC method
				int lenPassword = GetDlgItemText(hwnd, IDC_PASSWORD, (LPSTR) &passwd, MAXPWLEN+1);				
				int lenPassword2 = GetDlgItemText(hwnd, IDC_PASSWORD2, (LPSTR)&passwd2, MAXPWLEN + 1); //PGM

                bool bSecure = _this->m_server->Secure() ? true : false;
				if (Secure_old != bSecure) {
					//We changed the method to save the password
					//load passwords and encrypt the other method
					char password[MAXPWLEN];
					char password2[MAXPWLEN];
					_this->m_server->GetPassword(password);
					vncPasswd::ToText plain(password, Secure_old);
					_this->m_server->GetPassword2(password2);
					vncPasswd::ToText plain2(password2, Secure_old);
					memset(passwd, '\0', MAXPWLEN + 1); //PGM
					memset(passwd2, '\0', MAXPWLEN + 1); //PGM
					strcpy_s(passwd,plain);
					strcpy_s(passwd2, plain2);
					lenPassword = (int)strlen(passwd);
					lenPassword2 = (int)strlen(passwd2);
				}
				
				if (strcmp(passwd, "~~~~~~~~") != 0) {
					if (lenPassword == 0)
					{
						vncPasswd::FromClear crypt(_this->m_server->Secure());
						_this->m_server->SetPassword(crypt);
					}
					else
					{
						vncPasswd::FromText crypt(passwd, _this->m_server->Secure());
						_this->m_server->SetPassword(crypt);
					}
				}

				
				if (strcmp(passwd2, "~~~~~~~~") != 0) { //PGM
					if (lenPassword2 == 0) //PGM
					{ //PGM
						vncPasswd::FromClear crypt2(_this->m_server->Secure()); //PGM
						_this->m_server->SetPassword2(crypt2); //PGM
					} //PGM
					else //PGM
					{ //PGM
						vncPasswd::FromText crypt2(passwd2, _this->m_server->Secure()); //PGM
						_this->m_server->SetPassword2(crypt2); //PGM
					} //PGM
				} //PGM


				//avoid readonly and full passwd being set the same
				if (strcmp(passwd, "~~~~~~~~") != 0 && strcmp(passwd2, "~~~~~~~~") != 0) { 
					if (strcmp(passwd,passwd2)==0)
					{
						MessageBox(NULL,"View only and full password are the same\nView only ignored","Warning",0);
					}					
				} 

				// Save the new settings to the server
				int state = (int)SendDlgItemMessage(hwnd, IDC_PORTNO_AUTO, BM_GETCHECK, 0, 0);
				_this->m_server->SetAutoPortSelect(state == BST_CHECKED);

				// Save port numbers if we're not auto selecting
				if (!_this->m_server->AutoPortSelect()) {
					if ( SendDlgItemMessage(hwnd, IDC_SPECDISPLAY,
											BM_GETCHECK, 0, 0) == BST_CHECKED ) {
						// Display number was specified
						BOOL ok;
						UINT display = GetDlgItemInt(hwnd, IDC_DISPLAYNO, &ok, TRUE);
						if (ok)
							_this->m_server->SetPorts(DISPLAY_TO_PORT(display),
													  DISPLAY_TO_HPORT(display));
					} else {
						// Assuming that port numbers were specified
						BOOL ok1, ok2;
						UINT port_rfb = GetDlgItemInt(hwnd, IDC_PORTRFB, &ok1, TRUE);
						UINT port_http = GetDlgItemInt(hwnd, IDC_PORTHTTP, &ok2, TRUE);
						if (ok1 && ok2)
							_this->m_server->SetPorts(port_rfb, port_http);
					}
				}
				HWND hConnectSock = GetDlgItem(hwnd, IDC_CONNECT_SOCK);
				_this->m_server->SockConnect(
					SendMessage(hConnectSock, BM_GETCHECK, 0, 0) == BST_CHECKED
					);

				// Update display/port controls on pressing the "Apply" button
				if (LOWORD(wParam) == IDC_APPLY)
					_this->InitPortSettings(hwnd);

				

				HWND hConnectHTTP = GetDlgItem(hwnd, IDC_CONNECT_HTTP);
				_this->m_server->EnableHTTPConnect(
					SendMessage(hConnectHTTP, BM_GETCHECK, 0, 0) == BST_CHECKED
					);
				
				// Remote input stuff
				HWND hEnableRemoteInputs = GetDlgItem(hwnd, IDC_DISABLE_INPUTS);
				_this->m_server->EnableRemoteInputs(
					SendMessage(hEnableRemoteInputs, BM_GETCHECK, 0, 0) != BST_CHECKED
					);

				// Local input stuff
				HWND hDisableLocalInputs = GetDlgItem(hwnd, IDC_DISABLE_LOCAL_INPUTS);
				_this->m_server->DisableLocalInputs(
					SendMessage(hDisableLocalInputs, BM_GETCHECK, 0, 0) == BST_CHECKED
					);

				// japanese keyboard
				HWND hJapInputs = GetDlgItem(hwnd, IDC_JAP_INPUTS);
				_this->m_server->EnableJapInput(
					SendMessage(hJapInputs, BM_GETCHECK, 0, 0) == BST_CHECKED
					);

				// japanese keyboard
				HWND hUnicodeInputs = GetDlgItem(hwnd, IDC_UNICODE_INPUTS);
				_this->m_server->EnableUnicodeInput(
					SendMessage(hUnicodeInputs, BM_GETCHECK, 0, 0) == BST_CHECKED
					);

				HWND hwinhelper = GetDlgItem(hwnd, IDC_WIN8_HELPER);
				_this->m_server->Win8HelperEnabled(
					SendMessage(hwinhelper, BM_GETCHECK, 0, 0) == BST_CHECKED
					);

				// Wallpaper handling
				HWND hRemoveWallpaper = GetDlgItem(hwnd, IDC_REMOVE_WALLPAPER);
				_this->m_server->EnableRemoveWallpaper(
					SendMessage(hRemoveWallpaper, BM_GETCHECK, 0, 0) == BST_CHECKED
					);

				// Lock settings handling
				if (SendMessage(GetDlgItem(hwnd, IDC_LOCKSETTING_LOCK), BM_GETCHECK, 0, 0)
					== BST_CHECKED) {
					_this->m_server->SetLockSettings(1);
				} else if (SendMessage(GetDlgItem(hwnd, IDC_LOCKSETTING_LOGOFF), BM_GETCHECK, 0, 0)
					== BST_CHECKED) {
					_this->m_server->SetLockSettings(2);
				} else {
					_this->m_server->SetLockSettings(0);
				}

				if (SendMessage(GetDlgItem(hwnd, IDC_RADIONOTIFICATIONON), BM_GETCHECK, 0, 0)
					== BST_CHECKED) {
					_this->m_server->setNotificationSelection(0);
				}
				else if (SendMessage(GetDlgItem(hwnd, IDC_RADIONOTIFICATIONIFPROVIDED), BM_GETCHECK, 0, 0)
					== BST_CHECKED) {
					_this->m_server->setNotificationSelection(1);
				}

				if (SendMessage(GetDlgItem(hwnd, IDC_DREFUSE), BM_GETCHECK, 0, 0)
					== BST_CHECKED) {
					_this->m_server->SetQueryAccept(0);
				} else if (SendMessage(GetDlgItem(hwnd, IDC_DACCEPT), BM_GETCHECK, 0, 0)
					== BST_CHECKED) {
					_this->m_server->SetQueryAccept(1);
				} 

				if (SendMessage(GetDlgItem(hwnd, IDC_MAXREFUSE), BM_GETCHECK, 0, 0)
					== BST_CHECKED) {
					_this->m_server->setMaxViewerSetting(0);
				}
				else if (SendMessage(GetDlgItem(hwnd, IDC_MAXDISCONNECT), BM_GETCHECK, 0, 0)
					== BST_CHECKED) {
					_this->m_server->setMaxViewerSetting(1);
				}

				char maxViewerChar[256];
				strcpy_s(maxViewerChar, "128");				
				if (GetDlgItemText(hwnd, IDC_MAXVIEWERS, (LPSTR)&maxViewerChar, 256) == 0) {
					int value = atoi(maxViewerChar);
					if (value > 128) 
						value = 128;
					_this->m_server->setMaxViewers(value);
				}
				else
					_this->m_server->setMaxViewers(atoi(maxViewerChar));

				HWND hCollabo = GetDlgItem(hwnd, IDC_COLLABO);
				_this->m_server->setCollabo(
					SendMessage(hCollabo, BM_GETCHECK, 0, 0) == BST_CHECKED
					);

				HWND hwndDlg = GetDlgItem(hwnd, IDC_FRAME);
				_this->m_server->setFrame(
					SendMessage(hwndDlg, BM_GETCHECK, 0, 0) == BST_CHECKED
				);

				hwndDlg = GetDlgItem(hwnd, IDC_NOTIFOCATION);
				_this->m_server->setNotification(
					SendMessage(hwndDlg, BM_GETCHECK, 0, 0) == BST_CHECKED
				);

				hwndDlg = GetDlgItem(hwnd, IDC_OSD);
				_this->m_server->setOSD(
					SendMessage(hwndDlg, BM_GETCHECK, 0, 0) == BST_CHECKED
				);

				if (SendMessage(GetDlgItem(hwnd, IDC_MV1), BM_GETCHECK, 0, 0)
					== BST_CHECKED) {
					_this->m_server->SetConnectPriority(0);
				} else if (SendMessage(GetDlgItem(hwnd, IDC_MV2), BM_GETCHECK, 0, 0)
					== BST_CHECKED) {
					_this->m_server->SetConnectPriority(1);
				} 
				 else if (SendMessage(GetDlgItem(hwnd, IDC_MV3), BM_GETCHECK, 0, 0)
					== BST_CHECKED) {
					_this->m_server->SetConnectPriority(2);
				} else if (SendMessage(GetDlgItem(hwnd, IDC_MV4), BM_GETCHECK, 0, 0)
					== BST_CHECKED) {
					_this->m_server->SetConnectPriority(3);
				} 
				
				// Modif sf@2002 - v1.1.0
				HWND hFileTransfer = GetDlgItem(hwnd, IDC_FILETRANSFER);
				_this->m_server->EnableFileTransfer(SendMessage(hFileTransfer, BM_GETCHECK, 0, 0) == BST_CHECKED);

				HWND hFileTransferUserImp = GetDlgItem(hwnd, IDC_FTUSERIMPERSONATION_CHECK);
				_this->m_server->FTUserImpersonation(SendMessage(hFileTransferUserImp, BM_GETCHECK, 0, 0) == BST_CHECKED);

				HWND hBlank = GetDlgItem(hwnd, IDC_BLANK);
				_this->m_server->BlankMonitorEnabled(SendMessage(hBlank, BM_GETCHECK, 0, 0) == BST_CHECKED);
				HWND hBlank2 = GetDlgItem(hwnd, IDC_BLANK2); //PGM
				_this->m_server->BlankInputsOnly(SendMessage(hBlank2, BM_GETCHECK, 0, 0) == BST_CHECKED); //PGM				
				
				_this->m_server->SetLoopbackOk(IsDlgButtonChecked(hwnd, IDC_ALLOWLOOPBACK));
#ifdef IPV6V4
				_this->m_server->SetIPV6(IsDlgButtonChecked(hwnd, IDC_IPV6));
#endif
				_this->m_server->SetLoopbackOnly(IsDlgButtonChecked(hwnd, IDC_LOOPBACKONLY));

				_this->m_server->SetDisableTrayIcon(IsDlgButtonChecked(hwnd, IDC_DISABLETRAY));
				_this->m_server->SetRdpmode(IsDlgButtonChecked(hwnd, IDC_RDPMODE));
				_this->m_server->SetNoScreensaver(IsDlgButtonChecked(hwnd, IDC_NOSCREENSAVER));
				_this->m_allowshutdown=!IsDlgButtonChecked(hwnd, IDC_ALLOWSHUTDOWN);
				_this->m_alloweditclients=!IsDlgButtonChecked(hwnd, IDC_ALLOWEDITCLIENTS);
				_this->m_server->SetAllowEditClients(_this->m_alloweditclients);
				if (IsDlgButtonChecked(hwnd, IDC_LOG))
				{
					vnclog.SetMode(2);
					vnclog.SetLevel(10);
				}
				else
				{
					vnclog.SetMode(0);
				}
				if (IsDlgButtonChecked(hwnd, IDC_VIDEO))
				{
					vnclog.SetVideo(true);
				}
				else
				{
					vnclog.SetVideo(false);
				}
				// Modif sf@2002 - v1.1.0
				// Marscha@2004 - authSSP: moved MS-Logon checkbox back to admin props page
				// added New MS-Logon checkbox				

				HWND hMSLogon = GetDlgItem(hwnd, IDC_MSLOGON_CHECKD);
				_this->m_server->RequireMSLogon(SendMessage(hMSLogon, BM_GETCHECK, 0, 0) == BST_CHECKED);
				
				HWND hNewMSLogon = GetDlgItem(hwnd, IDC_NEW_MSLOGON);
				_this->m_server->SetNewMSLogon(SendMessage(hNewMSLogon, BM_GETCHECK, 0, 0) == BST_CHECKED);
				// Marscha@2004 - authSSP: end of change

				HWND hReverseAuth = GetDlgItem(hwnd, IDC_REVERSEAUTH);
				_this->m_server->SetReverseAuthRequired(SendMessage(hReverseAuth, BM_GETCHECK, 0, 0) == BST_CHECKED);

				int nDScale = GetDlgItemInt(hwnd, IDC_SCALE, NULL, FALSE);
				if (nDScale < 1 || nDScale > 9) nDScale = 1;
				_this->m_server->SetDefaultScale(nDScale);
				
				// sf@2002 - DSM Plugin loading
				// If Use plugin is checked, load the plugin if necessary
				if (SendMessage(GetDlgItem(hwnd, IDC_PLUGIN_CHECK), BM_GETCHECK, 0, 0) == BST_CHECKED)
				{
					TCHAR szPlugin[MAX_PATH];
					GetDlgItemText(hwnd, IDC_PLUGINS_COMBO, szPlugin, MAX_PATH);
					_this->m_server->SetDSMPluginName(szPlugin);
					_this->m_server->EnableDSMPlugin(true);
				}
				else // If Use plugin unchecked but the plugin is loaded, unload it
				{
					_this->m_server->EnableDSMPlugin(false);
					if (_this->m_server->GetDSMPluginPointer()->IsLoaded())
					{
						_this->m_server->GetDSMPluginPointer()->UnloadPlugin();
						_this->m_server->GetDSMPluginPointer()->SetEnabled(false);
					}	
				}

				//adzm 2010-05-12 - dsmplugin config
				_this->m_server->SetDSMPluginConfig(_this->m_pref_DSMPluginConfig);

				// Query Window options - Taken from TightVNC advanced properties
				char timeout[256];
				strcpy_s(timeout,"5");
				if (GetDlgItemText(hwnd, IDQUERYTIMEOUT, (LPSTR) &timeout, 256) == 0)
				    _this->m_server->SetQueryTimeout(atoi(timeout));
				else
				    _this->m_server->SetQueryTimeout(atoi(timeout));

				char disabletime[256];
				strcpy_s(disabletime,"5");
				if (GetDlgItemText(hwnd, IDC_QUERYDISABLETIME, (LPSTR) &disabletime, 256) == 0)
				    _this->m_server->SetQueryDisableTime(atoi(disabletime));
				else
				    _this->m_server->SetQueryDisableTime(atoi(disabletime));

				GetDlgItemText(hwnd, IDC_SERVICE_COMMANDLINE, _this->service_commandline, 1024);
				GetDlgItemText(hwnd, IDC_EDITQUERYTEXT, _this->accept_reject_mesg, 512);


				HWND hQuery = GetDlgItem(hwnd, IDQUERY);
				_this->m_server->SetQuerySetting((SendMessage(hQuery, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 4 : 2);

				// And to the registry

				/*if (!RunningAsAdministrator () && vncService::RunningAsService())
				{
					MessageBoxSecure(NULL,"Only admins are allowed to save","Warning", MB_OK | MB_ICONINFORMATION);
				}
				else*/
				{
				// Load the settings
				if (_this->m_fUseRegistry)
					_this->Save();
				else
					_this->SaveToIniFile();
				}

				// Was ok pressed?
				if (LOWORD(wParam) == IDOK)
				{
					// Yes, so close the dialog
					vnclog.Print(LL_INTINFO, VNCLOG("enddialog (OK)\n"));

					_this->m_returncode_valid = TRUE;

					EndDialog(hwnd, IDOK);
					_this->m_dlgvisible = FALSE;
				}

				_this->m_server->SetHookings();

				return TRUE;
			}

		case IDCANCEL:
			vnclog.Print(LL_INTINFO, VNCLOG("enddialog (CANCEL)\n"));

			_this->m_returncode_valid = TRUE;

			EndDialog(hwnd, IDCANCEL);
			_this->m_dlgvisible = FALSE;
			return TRUE;

	    // Added Jef Fix - 5 March 2008 paquette@atnetsend.net
        case IDC_BLANK:
            {
                // only enable alpha blanking if blanking is enabled
                HWND hBlank = ::GetDlgItem(hwnd, IDC_BLANK);               
                HWND hBlank2 = ::GetDlgItem(hwnd, IDC_BLANK2); //PGM
                ::EnableWindow(hBlank2, ::SendMessage(hBlank, BM_GETCHECK, 0, 0) == BST_CHECKED); //PGM
            }
            break;

        case IDC_BLANK2: //PGM
            { //PGM
                // only enable alpha blanking if Disable Only Inputs is disabled //PGM
                HWND hBlank = ::GetDlgItem(hwnd, IDC_BLANK2); //PGM              
            } //PGM
            break; //PGM

		case IDC_VIDEO:
			{
				if (IsDlgButtonChecked(hwnd, IDC_VIDEO))
				   {
					   EnableWindow(GetDlgItem(hwnd, IDC_EDIT_PATH), true);
					   
				   }
				   else
				   {
					   EnableWindow(GetDlgItem(hwnd, IDC_EDIT_PATH), false);
					   
				   }
				break;
			}

		case IDC_CLEAR:
			{
				vnclog.ClearAviConfig();
				break;
			}

		case IDC_CONNECT_SOCK:
			// TightVNC 1.2.7 method
			// The user has clicked on the socket connect tickbox
			{
				BOOL bConnectSock =
					(SendDlgItemMessage(hwnd, IDC_CONNECT_SOCK,
										BM_GETCHECK, 0, 0) == BST_CHECKED);

				EnableWindow(GetDlgItem(hwnd, IDC_PASSWORD), bConnectSock);

				HWND hPortNoAuto = GetDlgItem(hwnd, IDC_PORTNO_AUTO);
				EnableWindow(hPortNoAuto, bConnectSock);
				HWND hSpecDisplay = GetDlgItem(hwnd, IDC_SPECDISPLAY);
				EnableWindow(hSpecDisplay, bConnectSock);
				HWND hSpecPort = GetDlgItem(hwnd, IDC_SPECPORT);
				EnableWindow(hSpecPort, bConnectSock);

				EnableWindow(GetDlgItem(hwnd, IDC_DISPLAYNO), bConnectSock &&
					(SendMessage(hSpecDisplay, BM_GETCHECK, 0, 0) == BST_CHECKED));
				EnableWindow(GetDlgItem(hwnd, IDC_PORTRFB), bConnectSock &&
					(SendMessage(hSpecPort, BM_GETCHECK, 0, 0) == BST_CHECKED));
				EnableWindow(GetDlgItem(hwnd, IDC_PORTHTTP), bConnectSock &&
					(SendMessage(hSpecPort, BM_GETCHECK, 0, 0) == BST_CHECKED));
			}
			return TRUE;

		// TightVNC 1.2.7 method
		case IDC_PORTNO_AUTO:
			{
				EnableWindow(GetDlgItem(hwnd, IDC_DISPLAYNO), FALSE);
				EnableWindow(GetDlgItem(hwnd, IDC_PORTRFB), FALSE);
				EnableWindow(GetDlgItem(hwnd, IDC_PORTHTTP), FALSE);

				SetDlgItemText(hwnd, IDC_DISPLAYNO, "");
				SetDlgItemText(hwnd, IDC_PORTRFB, "");
				SetDlgItemText(hwnd, IDC_PORTHTTP, "");
			}
			return TRUE;

		case IDC_SPECDISPLAY:
			{
				EnableWindow(GetDlgItem(hwnd, IDC_DISPLAYNO), TRUE);
				EnableWindow(GetDlgItem(hwnd, IDC_PORTRFB), FALSE);
				EnableWindow(GetDlgItem(hwnd, IDC_PORTHTTP), FALSE);

				int display = PORT_TO_DISPLAY(_this->m_server->GetPort());
				if (display < 0 || display > 99)
					display = 0;
				SetDlgItemInt(hwnd, IDC_DISPLAYNO, display, FALSE);
				SetDlgItemInt(hwnd, IDC_PORTRFB, _this->m_server->GetPort(), FALSE);
				SetDlgItemInt(hwnd, IDC_PORTHTTP, _this->m_server->GetHttpPort(), FALSE);


				SetFocus(GetDlgItem(hwnd, IDC_DISPLAYNO));
				SendDlgItemMessage(hwnd, IDC_DISPLAYNO, EM_SETSEL, 0, (LPARAM)-1);
			}
			return TRUE;

		case IDC_SPECPORT:
			{
				EnableWindow(GetDlgItem(hwnd, IDC_DISPLAYNO), FALSE);
				EnableWindow(GetDlgItem(hwnd, IDC_PORTRFB), TRUE);
				EnableWindow(GetDlgItem(hwnd, IDC_PORTHTTP), TRUE);

				int d1 = PORT_TO_DISPLAY(_this->m_server->GetPort());
				int d2 = HPORT_TO_DISPLAY(_this->m_server->GetHttpPort());
				if (d1 == d2 && d1 >= 0 && d1 <= 99) {
					SetDlgItemInt(hwnd, IDC_DISPLAYNO, d1, FALSE);
				} else {
					SetDlgItemText(hwnd, IDC_DISPLAYNO, "");
				}
				SetDlgItemInt(hwnd, IDC_PORTRFB, _this->m_server->GetPort(), FALSE);
				SetDlgItemInt(hwnd, IDC_PORTHTTP, _this->m_server->GetHttpPort(), FALSE);


				SetFocus(GetDlgItem(hwnd, IDC_PORTRFB));
				SendDlgItemMessage(hwnd, IDC_PORTRFB, EM_SETSEL, 0, (LPARAM)-1);
			}
			return TRUE;

		// Query window option - Taken from TightVNC advanced properties code
		case IDQUERY:
			{
				HWND hQuery = GetDlgItem(hwnd, IDQUERY);
				BOOL queryon = (SendMessage(hQuery, BM_GETCHECK, 0, 0) == BST_CHECKED);
				EnableWindow(GetDlgItem(hwnd, IDQUERYTIMEOUT), queryon);
				EnableWindow(GetDlgItem(hwnd, IDC_QUERYDISABLETIME), queryon);
				EnableWindow(GetDlgItem(hwnd, IDC_DREFUSE), queryon);
				EnableWindow(GetDlgItem(hwnd, IDC_DACCEPT), queryon);
			}
			return TRUE;

		case IDC_STARTREP:
			{
				vncConnDialog *newconn = new vncConnDialog(_this->m_server);
				if (newconn)
				{
					newconn->DoDialog(true);
					// delete newconn; // NO ! Already done in vncConnDialog.
				}
			}

		// sf@2002 - DSM Plugin
		case IDC_PLUGIN_CHECK:
			{
				EnableWindow(GetDlgItem(hwnd, IDC_PLUGIN_BUTTON),
					SendMessage(GetDlgItem(hwnd, IDC_PLUGIN_CHECK), BM_GETCHECK, 0, 0) == BST_CHECKED);
			}
			return TRUE;
			// Marscha@2004 - authSSP: moved MSLogon checkbox back to admin props page
			// Reason: Different UI for old and new mslogon group config.
		case IDC_MSLOGON_CHECKD:
			{
				BOOL bMSLogonChecked =
				(SendDlgItemMessage(hwnd, IDC_MSLOGON_CHECKD,
										BM_GETCHECK, 0, 0) == BST_CHECKED);

				EnableWindow(GetDlgItem(hwnd, IDC_NEW_MSLOGON), bMSLogonChecked);
				EnableWindow(GetDlgItem(hwnd, IDC_MSLOGON), bMSLogonChecked);

			}
			return TRUE;
		case IDC_MSLOGON:
			{
				// Marscha@2004 - authSSP: if "New MS-Logon" is checked,
				// call vncEditSecurity from SecurityEditor.dll,
				// else call "old" dialog.
				BOOL bNewMSLogonChecked =
				(SendDlgItemMessage(hwnd, IDC_NEW_MSLOGON,
										BM_GETCHECK, 0, 0) == BST_CHECKED);
				if (bNewMSLogonChecked) {
					void winvncSecurityEditorHelper_as_admin();
						HANDLE hProcess,hPToken;
						DWORD id = vncService::GetExplorerLogonPid();
						if (id!=0) 
						{
							hProcess = OpenProcess(MAXIMUM_ALLOWED,FALSE,id);
							if (!hProcess) goto error;
							if(!OpenProcessToken(hProcess,TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY
													|TOKEN_DUPLICATE|TOKEN_ASSIGN_PRIMARY|TOKEN_ADJUST_SESSIONID
													|TOKEN_READ|TOKEN_WRITE,&hPToken)) break;

							char dir[MAX_PATH];
							char exe_file_name[MAX_PATH];
							GetModuleFileName(0, exe_file_name, MAX_PATH);
							strcpy_s(dir, exe_file_name);
							strcat_s(dir, " -securityeditorhelper");
				
							{
								STARTUPINFO          StartUPInfo;
								PROCESS_INFORMATION  ProcessInfo;
								ZeroMemory(&StartUPInfo,sizeof(STARTUPINFO));
								ZeroMemory(&ProcessInfo,sizeof(PROCESS_INFORMATION));
								StartUPInfo.wShowWindow = SW_SHOW;
								StartUPInfo.lpDesktop = "Winsta0\\Default";
								StartUPInfo.cb = sizeof(STARTUPINFO);
						
								CreateProcessAsUser(hPToken,NULL,dir,NULL,NULL,FALSE,DETACHED_PROCESS,NULL,NULL,&StartUPInfo,&ProcessInfo);
								DWORD errorcode=GetLastError();
                                if (ProcessInfo.hThread) CloseHandle(ProcessInfo.hThread);
                                if (ProcessInfo.hProcess) CloseHandle(ProcessInfo.hProcess);
								if (errorcode == 1314) goto error;
								break;
								error:
										winvncSecurityEditorHelper_as_admin();

							}
						}
				} else { 
					// Marscha@2004 - authSSP: end of change
					_this->m_vncauth.Init(_this->m_server);
					_this->m_vncauth.SetTemp(_this->m_Tempfile);
					_this->m_vncauth.Show(TRUE);
				}
			}
			return TRUE;
		case IDC_CHECKDRIVER:
			{
				CheckVideoDriver(1);
			}
			return TRUE;
		case IDC_PLUGIN_BUTTON:
			{
				HWND hPlugin = GetDlgItem(hwnd, IDC_PLUGIN_CHECK);
				if (SendMessage(hPlugin, BM_GETCHECK, 0, 0) == BST_CHECKED)
				{
					TCHAR szPlugin[MAX_PATH];
					GetDlgItemText(hwnd, IDC_PLUGINS_COMBO, szPlugin, MAX_PATH);
					if (!_this->m_server->GetDSMPluginPointer()->IsLoaded())
						_this->m_server->GetDSMPluginPointer()->LoadPlugin(szPlugin, false);
					else
					{
						// sf@2003 - We check if the loaded plugin is the same than
						// the currently selected one or not
						_this->m_server->GetDSMPluginPointer()->DescribePlugin();
						if (_stricmp(_this->m_server->GetDSMPluginPointer()->GetPluginFileName(), szPlugin))
						{
							_this->m_server->GetDSMPluginPointer()->UnloadPlugin();
							_this->m_server->GetDSMPluginPointer()->LoadPlugin(szPlugin, false);
						}
					}
				
					if (_this->m_server->GetDSMPluginPointer()->IsLoaded())
					{
						Secure_Save_Plugin_Config(szPlugin);
					}
					else
					{
						MessageBoxSecure(NULL, 
							sz_ID_PLUGIN_NOT_LOAD, 
							sz_ID_PLUGIN_LOADIN, MB_OK | MB_ICONEXCLAMATION );
					}
				}				
				return TRUE;
			}



		}
		break;
	}
	return 0;
}