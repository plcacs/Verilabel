void CtcpParser::packedReply(CoreNetwork *net, const QString &bufname, const QList<QByteArray> &replies) {
  QList<QByteArray> params;

  int answerSize = 0;
  for(int i = 0; i < replies.count(); i++) {
    answerSize += replies.at(i).size();
  }

  QByteArray quotedReply;
  quotedReply.reserve(answerSize);
  for(int i = 0; i < replies.count(); i++) {
    quotedReply.append(replies.at(i));
  }

  params << net->serverEncode(bufname) << quotedReply;
  // FIXME user proper event
  net->putCmd("NOTICE", params);
}