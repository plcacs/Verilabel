void FoFiType1::parse() {
  char *line, *line1, *p, *p2;
  char buf[256];
  char c;
  int n, code, i, j;
  char *tokptr;

  for (i = 1, line = (char *)file;
       i <= 100 && line && (!name || !encoding);
       ++i) {

    // get font name
    if (!name && !strncmp(line, "/FontName", 9)) {
      strncpy(buf, line, 255);
      buf[255] = '\0';
      if ((p = strchr(buf+9, '/')) &&
	  (p = strtok_r(p+1, " \t\n\r", &tokptr))) {
	name = copyString(p);
      }
      line = getNextLine(line);

    // get encoding
    } else if (!encoding &&
	       !strncmp(line, "/Encoding StandardEncoding def", 30)) {
      encoding = fofiType1StandardEncoding;
    } else if (!encoding &&
	       !strncmp(line, "/Encoding 256 array", 19)) {
      encoding = (char **)gmallocn(256, sizeof(char *));
      for (j = 0; j < 256; ++j) {
	encoding[j] = NULL;
      }
      for (j = 0, line = getNextLine(line);
	   j < 300 && line && (line1 = getNextLine(line));
	   ++j, line = line1) {
	if ((n = line1 - line) > 255) {
	  error(-1, "FoFiType1::parse a line has more than 255 characters, we don't support this");
	  n = 255;
	}
	strncpy(buf, line, n);
	buf[n] = '\0';
	for (p = buf; *p == ' ' || *p == '\t'; ++p) ;
	if (!strncmp(p, "dup", 3)) {
	  for (p += 3; *p == ' ' || *p == '\t'; ++p) ;
	  for (p2 = p; *p2 >= '0' && *p2 <= '9'; ++p2) ;
	  if (*p2) {
	    c = *p2; // store it so we can recover it after atoi
	    *p2 = '\0'; // terminate p so atoi works
	    code = atoi(p);
	    *p2 = c;
	    if (code == 8 && *p2 == '#') {
	      code = 0;
	      for (++p2; *p2 >= '0' && *p2 <= '7'; ++p2) {
		code = code * 8 + (*p2 - '0');
	      }
	    }
	    if (code < 256) {
	      for (p = p2; *p == ' ' || *p == '\t'; ++p) ;
	      if (*p == '/') {
		++p;
		for (p2 = p; *p2 && *p2 != ' ' && *p2 != '\t'; ++p2) ;
		c = *p2; // store it so we can recover it after copyString
		*p2 = '\0'; // terminate p so copyString works
		encoding[code] = copyString(p);
		*p2 = c;
		p = p2;
		for (; *p == ' ' || *p == '\t'; ++p); // eat spaces between string and put
		if (!strncmp(p, "put", 3)) {
		  // eat put and spaces and newlines after put
		  for (p += 3; *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'; ++p);
		  if (*p)
		  {
		    // there is still something after the definition
		    // there might be another definition in this line
		    // so move line1 to the end of our parsing
		    // so we start in the potential next definition in the next loop
		    line1 = &line[p - buf];
		  }
		} else {
		  error(-1, "FoFiType1::parse no put after dup");
		}
	      }
	    }
	  }
	} else {
	  if (strtok_r(buf, " \t", &tokptr) &&
	      (p = strtok_r(NULL, " \t\n\r", &tokptr)) && !strcmp(p, "def")) {
	    break;
	  }
	}
      }
      //~ check for getinterval/putinterval junk

    } else {
      line = getNextLine(line);
    }
  }

  parsed = gTrue;
}