int HttpFileImpl::save(const std::string &path) const
{
    assert(!path.empty());
    if (fileName_.empty())
        return -1;
    filesystem::path fsPath(utils::toNativePath(path));
    if (!fsPath.is_absolute() &&
        (!fsPath.has_parent_path() ||
         (fsPath.begin()->string() != "." && fsPath.begin()->string() != "..")))
    {
        filesystem::path fsUploadPath(utils::toNativePath(
            HttpAppFrameworkImpl::instance().getUploadPath()));
        fsPath = fsUploadPath / fsPath;
    }
    filesystem::path fsFileName(utils::toNativePath(fileName_));
    if (!filesystem::exists(fsPath))
    {
        LOG_TRACE << "create path:" << fsPath;
        drogon::error_code err;
        filesystem::create_directories(fsPath, err);
        if (err)
        {
            LOG_SYSERR;
            return -1;
        }
    }
    return saveTo(fsPath / fsFileName);
}