CString CWebSock::GetSkinPath(const CString& sSkinName) {
    CString sRet = CZNC::Get().GetZNCPath() + "/webskins/" + sSkinName;

    if (!CFile::IsDir(sRet)) {
        sRet = CZNC::Get().GetCurPath() + "/webskins/" + sSkinName;

        if (!CFile::IsDir(sRet)) {
            sRet = CString(_SKINDIR_) + "/" + sSkinName;
        }
    }

    return sRet + "/";
}