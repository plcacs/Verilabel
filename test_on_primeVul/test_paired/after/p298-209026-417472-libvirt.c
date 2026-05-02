virNodeDeviceGetMdevTypesCaps(const char *sysfspath,
                              virMediatedDeviceTypePtr **mdev_types,
                              size_t *nmdev_types)
{
    virMediatedDeviceTypePtr *types = NULL;
    size_t ntypes = 0;
    size_t i;

    /* this could be a refresh, so clear out the old data */
    for (i = 0; i < *nmdev_types; i++)
       virMediatedDeviceTypeFree((*mdev_types)[i]);
    VIR_FREE(*mdev_types);
    *nmdev_types = 0;

    if (virMediatedDeviceGetMdevTypes(sysfspath, &types, &ntypes) < 0)
        return -1;

    *mdev_types = g_steal_pointer(&types);
    *nmdev_types = ntypes;

    return 0;
}