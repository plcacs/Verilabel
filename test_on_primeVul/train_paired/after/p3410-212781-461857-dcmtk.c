OFCondition DcmSCP::listen()
{

  // make sure not to let dcmdata remove trailing blank padding or perform other
  // manipulations. We want to see the real data.
  dcmEnableAutomaticInputDataCorrection.set( OFFalse );

  OFCondition cond = EC_Normal;
  // Make sure data dictionary is loaded.
  if( !dcmDataDict.isDictionaryLoaded() )
    DCMNET_WARN("No data dictionary loaded, check environment variable: " << DCM_DICT_ENVIRONMENT_VARIABLE);

#ifndef DISABLE_PORT_PERMISSION_CHECK
#ifdef HAVE_GETEUID
  // If port is privileged we must be as well.
  if( m_cfg->getPort() < 1024 && geteuid() != 0 )
  {
    DCMNET_ERROR("No privileges to open this network port (" << m_cfg->getPort() << ")");
    return NET_EC_InsufficientPortPrivileges;
  }
#endif
#endif

  // Initialize network, i.e. create an instance of T_ASC_Network*.
  T_ASC_Network *network = NULL;
  cond = ASC_initializeNetwork( NET_ACCEPTOR, OFstatic_cast(int, m_cfg->getPort()), m_cfg->getACSETimeout(), &network );
  if( cond.bad() )
    return cond;

#if defined(HAVE_SETUID) && defined(HAVE_GETUID)
  // Return to normal uid so that we can't do too much damage in case
  // things go very wrong. Only works if the program is setuid root,
  // and run by another user. Running as root user may be
  // potentially disastrous if this program screws up badly.
  if ((setuid(getuid()) == -1) && (errno == EAGAIN))
  {
      DCMNET_ERROR("setuid() failed, maximum number of processes/threads for uid already running.");
      return NET_EC_InsufficientPortPrivileges;
  }
#endif

  // If we get to this point, the entire initialization process has been completed
  // successfully. Now, we want to start handling all incoming requests. Since
  // this activity is supposed to represent a server process, we do not want to
  // terminate this activity (unless indicated by the stopAfterCurrentAssociation()
  // method). Hence, create an infinite while-loop.
  while( cond.good() && !stopAfterCurrentAssociation() )
  {
    // Wait for an association and handle the requests of
    // the calling applications correspondingly.
    cond = waitForAssociationRQ(network);
  }
  // Drop the network, i.e. free memory of T_ASC_Network* structure. This call
  // is the counterpart of ASC_initializeNetwork(...) which was called above.
  cond = ASC_dropNetwork( &network );
  network = NULL;

  // return ok
  return cond;
}