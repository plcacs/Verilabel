char *VDir::MapPathA(const char *pInName)
{   /*
     * possiblities -- relative path or absolute path with or without drive letter
     * OR UNC name
     */
    char szBuffer[(MAX_PATH+1)*2];
    char szlBuf[MAX_PATH+1];
    int length = strlen(pInName);

    if (!length)
	return (char*)pInName;

    if (length > MAX_PATH) {
	strncpy(szlBuf, pInName, MAX_PATH);
	if (IsPathSep(pInName[0]) && !IsPathSep(pInName[1])) {   
	    /* absolute path - reduce length by 2 for drive specifier */
	    szlBuf[MAX_PATH-2] = '\0';
	}
	else
	    szlBuf[MAX_PATH] = '\0';
	pInName = szlBuf;
    }
    /* strlen(pInName) is now <= MAX_PATH */

    if (pInName[1] == ':') {
	/* has drive letter */
	if (IsPathSep(pInName[2])) {
	    /* absolute with drive letter */
	    DoGetFullPathNameA((char*)pInName, sizeof(szLocalBufferA), szLocalBufferA);
	}
	else {
	    /* relative path with drive letter */
	    strcpy(szBuffer, GetDirA(DriveIndex(*pInName)));
	    strcat(szBuffer, &pInName[2]);
	    if(strlen(szBuffer) > MAX_PATH)
		szBuffer[MAX_PATH] = '\0';

	    DoGetFullPathNameA(szBuffer, sizeof(szLocalBufferA), szLocalBufferA);
	}
    }
    else {
	/* no drive letter */
	if (IsPathSep(pInName[1]) && IsPathSep(pInName[0])) {
	    /* UNC name */
	    DoGetFullPathNameA((char*)pInName, sizeof(szLocalBufferA), szLocalBufferA);
	}
	else {
	    strcpy(szBuffer, GetDefaultDirA());
	    if (IsPathSep(pInName[0])) {
		/* absolute path */
		strcpy(&szBuffer[2], pInName);
		DoGetFullPathNameA(szBuffer, sizeof(szLocalBufferA), szLocalBufferA);
	    }
	    else {
		/* relative path */
		if (IsSpecialFileName(pInName)) {
		    return (char*)pInName;
		}
		else {
		    strcat(szBuffer, pInName);
		    if (strlen(szBuffer) > MAX_PATH)
			szBuffer[MAX_PATH] = '\0';

		    DoGetFullPathNameA(szBuffer, sizeof(szLocalBufferA), szLocalBufferA);
		}
	    }
	}
    }

    return szLocalBufferA;
}