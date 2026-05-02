WCHAR* VDir::MapPathW(const WCHAR *pInName)
{   /*
     * possiblities -- relative path or absolute path with or without drive letter
     * OR UNC name
     */
    WCHAR szBuffer[(MAX_PATH+1)*2];
    WCHAR szlBuf[MAX_PATH+1];
    int length = wcslen(pInName);

    if (!length)
	return (WCHAR*)pInName;

    if (length > MAX_PATH) {
	wcsncpy(szlBuf, pInName, MAX_PATH);
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
	    DoGetFullPathNameW((WCHAR*)pInName, (sizeof(szLocalBufferW)/sizeof(WCHAR)), szLocalBufferW);
	}
	else {
	    /* relative path with drive letter */
	    wcscpy(szBuffer, GetDirW(DriveIndex((char)*pInName)));
	    wcscat(szBuffer, &pInName[2]);
	    if(wcslen(szBuffer) > MAX_PATH)
		szBuffer[MAX_PATH] = '\0';

	    DoGetFullPathNameW(szBuffer, (sizeof(szLocalBufferW)/sizeof(WCHAR)), szLocalBufferW);
	}
    }
    else {
	/* no drive letter */
	if (IsPathSep(pInName[1]) && IsPathSep(pInName[0])) {
	    /* UNC name */
	    DoGetFullPathNameW((WCHAR*)pInName, (sizeof(szLocalBufferW)/sizeof(WCHAR)), szLocalBufferW);
	}
	else {
	    wcscpy(szBuffer, GetDefaultDirW());
	    if (IsPathSep(pInName[0])) {
		/* absolute path */
		wcscpy(&szBuffer[2], pInName);
		DoGetFullPathNameW(szBuffer, (sizeof(szLocalBufferW)/sizeof(WCHAR)), szLocalBufferW);
	    }
	    else {
		/* relative path */
		if (IsSpecialFileName(pInName)) {
		    return (WCHAR*)pInName;
		}
		else {
		    wcscat(szBuffer, pInName);
		    if (wcslen(szBuffer) > MAX_PATH)
			szBuffer[MAX_PATH] = '\0';

		    DoGetFullPathNameW(szBuffer, (sizeof(szLocalBufferW)/sizeof(WCHAR)), szLocalBufferW);
		}
	    }
	}
    }
    return szLocalBufferW;
}