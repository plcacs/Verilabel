nwfilterConnectNumOfNWFilters(virConnectPtr conn)
{
    int ret;
    if (virConnectNumOfNWFiltersEnsureACL(conn) < 0)
        return -1;

    nwfilterDriverLock();
    ret = virNWFilterObjListNumOfNWFilters(driver->nwfilters, conn,
                                           virConnectNumOfNWFiltersCheckACL);
    nwfilterDriverUnlock();
    return ret;
}