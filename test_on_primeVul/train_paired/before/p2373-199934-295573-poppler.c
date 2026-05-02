void FontInfoScanner::scanFonts(XRef *xrefA, Dict *resDict, std::vector<FontInfo*> *fontsList) {
  GfxFontDict *gfxFontDict;
  GfxFont *font;

  // scan the fonts in this resource dictionary
  gfxFontDict = nullptr;
  const Object &fontObj = resDict->lookupNF("Font");
  if (fontObj.isRef()) {
    Object obj2 = fontObj.fetch(xrefA);
    if (obj2.isDict()) {
      Ref r = fontObj.getRef();
      gfxFontDict = new GfxFontDict(xrefA, &r, obj2.getDict());
    }
  } else if (fontObj.isDict()) {
    gfxFontDict = new GfxFontDict(xrefA, nullptr, fontObj.getDict());
  }
  if (gfxFontDict) {
    for (int i = 0; i < gfxFontDict->getNumFonts(); ++i) {
      if ((font = gfxFontDict->getFont(i))) {
        Ref fontRef = *font->getID();

        // add this font to the list if not already found
        if (fonts.find(fontRef.num) == fonts.end()) {
	  fontsList->push_back(new FontInfo(font, xrefA));
          fonts.insert(fontRef.num);
        }
      }
    }
    delete gfxFontDict;
  }

  // recursively scan any resource dictionaries in objects in this
  // resource dictionary
  const char *resTypes[] = { "XObject", "Pattern" };
  for (unsigned int resType = 0; resType < sizeof(resTypes) / sizeof(resTypes[0]); ++resType) {
    Object objDict = resDict->lookup(resTypes[resType]);
    if (objDict.isDict()) {
      for (int i = 0; i < objDict.dictGetLength(); ++i) {
        const Object &dictObjI = objDict.dictGetValNF(i);
        if (dictObjI.isRef()) {
          // check for an already-seen object
          const Ref r = dictObjI.getRef();
          if (visitedObjects.find(r.num) != visitedObjects.end()) {
            continue;
          }

          visitedObjects.insert(r.num);
        }

        Object obj2 = dictObjI.fetch(xrefA);
        if (obj2.isStream()) {
          Object resObj = obj2.streamGetDict()->lookup("Resources");
          if (resObj.isDict() && resObj.getDict() != resDict) {
            scanFonts(xrefA, resObj.getDict(), fontsList);
          }
        }
      }
    }
  }
}