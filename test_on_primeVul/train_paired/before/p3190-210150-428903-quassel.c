void CtcpParser::packedReply(CoreNetwork *net, const QString &bufname, const QList<QByteArray> &replies) {
  QList<QByteArray> params;

  int answerSize = 0;
  for(int i = 0; i < replies.count(); i++) {
    answerSize += replies.at(i).size();
  }

  QByteArray quotedReply(answerSize, 0);
  int nextPos = 0;
  QByteArray &reply = quotedReply;
  for(int i = 0; i < replies.count(); i++) {
    reply = replies.at(i);
    quotedReply.replace(nextPos, reply.size(), reply);
    nextPos += reply.size();
  }

  params << net->serverEncode(bufname) << quotedReply;
  // FIXME user proper event
  net->putCmd("NOTICE", params);
}