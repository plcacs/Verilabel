void FrameFactory::rebuildAggregateFrames(ID3v2::Tag *tag) const
{
  if(tag->header()->majorVersion() < 4 &&
     tag->frameList("TDRC").size() == 1 &&
     tag->frameList("TDAT").size() == 1)
  {
    TextIdentificationFrame *tdrc =
      static_cast<TextIdentificationFrame *>(tag->frameList("TDRC").front());
    UnknownFrame *tdat = static_cast<UnknownFrame *>(tag->frameList("TDAT").front());

    if(tdrc->fieldList().size() == 1 &&
       tdrc->fieldList().front().size() == 4 &&
       tdat->data().size() >= 5)
    {
      String date(tdat->data().mid(1), String::Type(tdat->data()[0]));
      if(date.length() == 4) {
        tdrc->setText(tdrc->toString() + '-' + date.substr(2, 2) + '-' + date.substr(0, 2));
        if(tag->frameList("TIME").size() == 1) {
          UnknownFrame *timeframe = static_cast<UnknownFrame *>(tag->frameList("TIME").front());
          if(timeframe->data().size() >= 5) {
            String time(timeframe->data().mid(1), String::Type(timeframe->data()[0]));
            if(time.length() == 4) {
              tdrc->setText(tdrc->toString() + 'T' + time.substr(0, 2) + ':' + time.substr(2, 2));
            }
          }
        }
      }
    }
  }
}