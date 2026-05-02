bool WindowsServiceControl::install( const QString& filePath, const QString& displayName  )
{
	m_serviceHandle = CreateService(
				m_serviceManager,		// SCManager database
				WindowsCoreFunctions::toConstWCharArray( m_name ),	// name of service
				WindowsCoreFunctions::toConstWCharArray( displayName ),// name to display
				SERVICE_ALL_ACCESS,	// desired access
				SERVICE_WIN32_OWN_PROCESS,
				// service type
				SERVICE_AUTO_START,	// start type
				SERVICE_ERROR_NORMAL,	// error control type
				WindowsCoreFunctions::toConstWCharArray( filePath ),		// service's binary
				nullptr,			// no load ordering group
				nullptr,			// no tag identifier
				L"Tcpip\0RpcSs\0\0",		// dependencies
				nullptr,			// LocalSystem account
				nullptr );			// no password

	if( m_serviceHandle == nullptr )
	{
		const auto error = GetLastError();
		if( error == ERROR_SERVICE_EXISTS )
		{
			vCritical() << qUtf8Printable( tr( "The service \"%1\" is already installed." ).arg( m_name ) );
		}
		else
		{
			vCritical() << qUtf8Printable( tr( "The service \"%1\" could not be installed." ).arg( m_name ) );
		}

		return false;
	}

	SC_ACTION serviceActions;
	serviceActions.Delay = 10000;
	serviceActions.Type = SC_ACTION_RESTART;

	SERVICE_FAILURE_ACTIONS serviceFailureActions;
	serviceFailureActions.dwResetPeriod = 0;
	serviceFailureActions.lpRebootMsg = nullptr;
	serviceFailureActions.lpCommand = nullptr;
	serviceFailureActions.lpsaActions = &serviceActions;
	serviceFailureActions.cActions = 1;
	ChangeServiceConfig2( m_serviceHandle, SERVICE_CONFIG_FAILURE_ACTIONS, &serviceFailureActions );

	// Everything went fine
	vInfo() << qUtf8Printable( tr( "The service \"%1\" has been installed successfully." ).arg( m_name ) );

	return true;
}