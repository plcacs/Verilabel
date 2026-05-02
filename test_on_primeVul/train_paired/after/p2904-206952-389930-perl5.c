char *VDir::MapPathA(const char *pInName)
{   /*
     * possiblities -- relative path or absolute path with or without drive letter
     * OR UNC name
     */
    int driveIndex;
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

    if (length > 1 && pInName[1] == ':') {
	/* has drive letter */
	if (length > 2 && IsPathSep(pInName[2])) {
	    /* absolute with drive letter */
	    DoGetFullPathNameA((char*)pInName, sizeof(szLocalBufferA), szLocalBufferA);
	}
	else {
	    /* relative path with drive letter */
            driveIndex = DriveIndex(*pInName);
            if (driveIndex < 0 || driveIndex >= driveLetterCount)
                return (char *)pInName;
	    strcpy(szBuffer, GetDirA(driveIndex));
	    strcat(szBuffer, &pInName[2]);
	    if(strlen(szBuffer) > MAX_PATH)
		szBuffer[MAX_PATH] = '\0';

	    DoGetFullPathNameA(szBuffer, sizeof(szLocalBufferA), szLocalBufferA);
	}
    }
    else {
	/* no drive letter */
	if (length > 1 && IsPathSep(pInName[1]) && IsPathSep(pInName[0])) {
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