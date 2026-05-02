OFCondition WlmActivityManager::StartProvidingService()
// Date         : December 10, 2001
// Author       : Thomas Wilkens
// Task         : Starts providing the implemented service for calling SCUs.
//                After having created an instance of this class, this function
//                shall be called from main.
// Parameters   : none.
// Return Value : Return value that is supposed to be returned from main().
{
  OFCondition cond = EC_Normal;
  T_ASC_Network *net = NULL;

  // Make sure data dictionary is loaded.
  if( !dcmDataDict.isDictionaryLoaded() )
  {
    DCMWLM_WARN("no data dictionary loaded, check environment variable: " << DCM_DICT_ENVIRONMENT_VARIABLE);
  }

#ifndef DISABLE_PORT_PERMISSION_CHECK
#ifdef HAVE_GETEUID
  // If port is privileged we must be as well.
  if( opt_port < 1024 && geteuid() != 0 )
    return( WLM_EC_InsufficientPortPrivileges );
#endif
#endif

#ifdef _WIN32
  /* if this process was started by CreateProcess, opt_forkedChild is set */
  if (opt_forkedChild)
  {
    /* tell dcmnet DUL about child process status, too */
    DUL_markProcessAsForkedChild();

    char buf[256];
    DWORD bytesRead = 0;
    HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);

    // read socket handle number from stdin, i.e. the anonymous pipe
    // to which our parent process has written the handle number.
    if (ReadFile(hStdIn, buf, sizeof(buf), &bytesRead, NULL))
    {
      // make sure buffer is zero terminated
      buf[bytesRead] = '\0';
        dcmExternalSocketHandle.set(atoi(buf));
    }
    else
    {
      DCMWLM_ERROR("cannot read socket handle: " << GetLastError());
      exit(0);
    }
  }
  else
  {
    // parent process
    if (!opt_singleProcess)
      DUL_requestForkOnTransportConnectionReceipt(cmd_argc, cmd_argv);
  }
#endif

  // Initialize network, i.e. create an instance of T_ASC_Network*.
  cond = ASC_initializeNetwork( NET_ACCEPTOR, OFstatic_cast(int, opt_port), opt_acse_timeout, &net );
  if( cond.bad() ) return( WLM_EC_InitializationOfNetworkConnectionFailed );

#if defined(HAVE_SETUID) && defined(HAVE_GETUID)
  // Return to normal uid so that we can't do too much damage in case
  // things go very wrong. Only works if the program is setuid root,
  // and run by another user. Running as root user may be
  // potentially disasterous if this program screws up badly.
  if ((setuid(getuid()) == -1) && (errno == EAGAIN))
  {
      DCMWLM_ERROR("setuid() failed, maximum number of processes/threads for uid already running.");
      return WLM_EC_InitializationOfNetworkConnectionFailed;
  }
#endif

  // If we get to this point, the entire initialization process has been completed
  // successfully. Now, we want to start handling all incoming requests. Since
  // this activity is supposed to represent a server process, we do not want to
  // terminate this activity. Hence, create an endless while-loop.
  while( cond.good() )
  {
    // Wait for an association and handle the requests of
    // the calling applications correspondingly.
    cond = WaitForAssociation( net );

    // Clean up any child processes if the execution is not limited to a single process.
    // (On windows platform, childs are not handled via the process table,
    // so there's no need to clean up children)
#ifdef HAVE_FORK
    if( !opt_singleProcess )
      CleanChildren();
#elif defined(_WIN32)
    // if running in multi-process mode, always terminate child after one association
    // for unix, this is done in WaitForAssociation() with exit()
    if (DUL_processIsForkedChild()) break;
#endif
  }
  // Drop the network, i.e. free memory of T_ASC_Network* structure. This call
  // is the counterpart of ASC_initializeNetwork(...) which was called above.
  cond = ASC_dropNetwork( &net );
  if( cond.bad() ) return( WLM_EC_TerminationOfNetworkConnectionFailed );

  // return ok
  return( EC_Normal );
}