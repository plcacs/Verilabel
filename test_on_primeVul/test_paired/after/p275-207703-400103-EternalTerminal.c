set<int> PipeSocketHandler::listen(const SocketEndpoint& endpoint) {
  lock_guard<std::recursive_mutex> guard(globalMutex);

  string pipePath = endpoint.name();
  if (pipeServerSockets.find(pipePath) != pipeServerSockets.end()) {
    throw runtime_error("Tried to listen twice on the same path");
  }

  sockaddr_un local;

  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  FATAL_FAIL(fd);
  initServerSocket(fd);
  local.sun_family = AF_UNIX; /* local is declared before socket() ^ */
  strncpy(local.sun_path, pipePath.c_str(), sizeof(local.sun_path));
  unlink(local.sun_path);

  FATAL_FAIL(::bind(fd, (struct sockaddr*)&local, sizeof(sockaddr_un)));
  ::listen(fd, 5);
#ifndef WIN32
  FATAL_FAIL(::chmod(local.sun_path, S_IRUSR | S_IWUSR | S_IXUSR));
#endif

  pipeServerSockets[pipePath] = set<int>({fd});
  return pipeServerSockets[pipePath];
}