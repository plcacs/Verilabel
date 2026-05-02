void PDFDoc::markObject (Object* obj, XRef *xRef, XRef *countRef, unsigned int numOffset, int oldRefNum, int newRefNum, std::set<Dict*> *alreadyMarkedDicts)
{
  Array *array;

  switch (obj->getType()) {
    case objArray:
      array = obj->getArray();
      for (int i=0; i<array->getLength(); i++) {
        Object obj1 = array->getNF(i).copy();
        markObject(&obj1, xRef, countRef, numOffset, oldRefNum, newRefNum);
      }
      break;
    case objDict:
      markDictionnary (obj->getDict(), xRef, countRef, numOffset, oldRefNum, newRefNum, alreadyMarkedDicts);
      break;
    case objStream: 
      {
        Stream *stream = obj->getStream();
        markDictionnary (stream->getDict(), xRef, countRef, numOffset, oldRefNum, newRefNum, alreadyMarkedDicts);
      }
      break;
    case objRef:
      {
        if (obj->getRef().num + (int) numOffset >= xRef->getNumObjects() || xRef->getEntry(obj->getRef().num + numOffset)->type == xrefEntryFree) {
          if (getXRef()->getEntry(obj->getRef().num)->type == xrefEntryFree) {
            return;  // already marked as free => should be replaced
          }
          xRef->add(obj->getRef().num + numOffset, obj->getRef().gen, 0, true);
          if (getXRef()->getEntry(obj->getRef().num)->type == xrefEntryCompressed) {
            xRef->getEntry(obj->getRef().num + numOffset)->type = xrefEntryCompressed;
          }
        }
        if (obj->getRef().num + (int) numOffset >= countRef->getNumObjects() || 
            countRef->getEntry(obj->getRef().num + numOffset)->type == xrefEntryFree)
        {
          countRef->add(obj->getRef().num + numOffset, 1, 0, true);
        } else {
          XRefEntry *entry = countRef->getEntry(obj->getRef().num + numOffset);
          entry->gen++;
          if (entry->gen > 9)
            break;
        } 
        Object obj1 = getXRef()->fetch(obj->getRef());
        markObject(&obj1, xRef, countRef, numOffset, oldRefNum, newRefNum);
      }
      break;
    default:
      break;
  }
}