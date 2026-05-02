int HttpFileImpl::saveAs(const std::string &fileName) const
{
    assert(!fileName.empty());
    filesystem::path fsFileName(utils::toNativePath(fileName));
    if (!fsFileName.is_absolute() && (!fsFileName.has_parent_path() ||
                                      (fsFileName.begin()->string() != "." &&
                                       fsFileName.begin()->string() != "..")))
    {
        filesystem::path fsUploadPath(utils::toNativePath(
            HttpAppFrameworkImpl::instance().getUploadPath()));
        fsFileName = fsUploadPath / fsFileName;
    }
    if (fsFileName.has_parent_path() &&
        !filesystem::exists(fsFileName.parent_path()))
    {
        LOG_TRACE << "create path:" << fsFileName.parent_path();
        drogon::error_code err;
        filesystem::create_directories(fsFileName.parent_path(), err);
        if (err)
        {
            LOG_SYSERR;
            return -1;
        }
    }
    return saveTo(fsFileName);
}