void CharCodeToUnicode::addMapping(CharCode code, char *uStr, int n,
				   int offset) {
  CharCode oldLen, i;
  Unicode u;
  char uHex[5];
  int j;

  if (code >= mapLen) {
    oldLen = mapLen;
    mapLen = (code + 256) & ~255;
    if (unlikely(code >= mapLen)) {
      error(-1, "Illegal code value in CharCodeToUnicode::addMapping");
      return;
    } else {
      map = (Unicode *)greallocn(map, mapLen, sizeof(Unicode));
      for (i = oldLen; i < mapLen; ++i) {
        map[i] = 0;
      }
    }
  }
  if (n <= 4) {
    if (sscanf(uStr, "%x", &u) != 1) {
      error(-1, "Illegal entry in ToUnicode CMap");
      return;
    }
    map[code] = u + offset;
  } else {
    if (sMapLen >= sMapSize) {
      sMapSize = sMapSize + 16;
      sMap = (CharCodeToUnicodeString *)
	       greallocn(sMap, sMapSize, sizeof(CharCodeToUnicodeString));
    }
    map[code] = 0;
    sMap[sMapLen].c = code;
    sMap[sMapLen].len = n / 4;
    for (j = 0; j < sMap[sMapLen].len && j < maxUnicodeString; ++j) {
      strncpy(uHex, uStr + j*4, 4);
      uHex[4] = '\0';
      if (sscanf(uHex, "%x", &sMap[sMapLen].u[j]) != 1) {
	error(-1, "Illegal entry in ToUnicode CMap");
      }
    }
    sMap[sMapLen].u[sMap[sMapLen].len - 1] += offset;
    ++sMapLen;
  }
}