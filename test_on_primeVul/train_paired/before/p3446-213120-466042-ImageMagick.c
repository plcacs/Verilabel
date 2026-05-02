static inline void CheckEventLogging()
{
  /*
    Are we logging events?
  */
  if (IsLinkedListEmpty(log_cache) != MagickFalse)
    event_logging=MagickFalse;
  else
    {
      LogInfo
        *p;

      ResetLinkedListIterator(log_cache);
      p=(LogInfo *) GetNextValueInLinkedList(log_cache);
      event_logging=p->event_mask != NoEvents ? MagickTrue: MagickFalse;
    }
}