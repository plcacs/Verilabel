nwfilterConnectNumOfNWFilters(virConnectPtr conn)
{
    if (virConnectNumOfNWFiltersEnsureACL(conn) < 0)
        return -1;

    return virNWFilterObjListNumOfNWFilters(driver->nwfilters, conn,
                                        virConnectNumOfNWFiltersCheckACL);
}