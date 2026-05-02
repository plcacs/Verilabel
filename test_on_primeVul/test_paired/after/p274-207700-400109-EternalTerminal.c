TerminalUserInfo UserTerminalRouter::getInfoForId(const string &id) {
  lock_guard<recursive_mutex> guard(routerMutex);
  auto it = idInfoMap.find(id);
  if (it == idInfoMap.end()) {
    STFATAL << " Tried to read from an id that no longer exists";
  }
  return it->second;
}