ProcXkbSetMap(ClientPtr client)
{
    DeviceIntPtr dev;
    char *tmp;
    int rc;

    REQUEST(xkbSetMapReq);
    REQUEST_AT_LEAST_SIZE(xkbSetMapReq);

    if (!(client->xkbClientFlags & _XkbClientInitialized))
        return BadAccess;

    CHK_KBD_DEVICE(dev, stuff->deviceSpec, client, DixManageAccess);
    CHK_MASK_LEGAL(0x01, stuff->present, XkbAllMapComponentsMask);

    tmp = (char *) &stuff[1];

    /* Check if we can to the SetMap on the requested device. If this
       succeeds, do the same thing for all extension devices (if needed).
       If any of them fails, fail.  */
    rc = _XkbSetMapChecks(client, dev, stuff, tmp, TRUE);

    if (rc != Success)
        return rc;

    DeviceIntPtr master = GetMaster(dev, MASTER_KEYBOARD);

    if (stuff->deviceSpec == XkbUseCoreKbd) {
        DeviceIntPtr other;

        for (other = inputInfo.devices; other; other = other->next) {
            if ((other != dev) && other->key && !IsMaster(other) &&
                GetMaster(other, MASTER_KEYBOARD) == dev) {
                rc = XaceHook(XACE_DEVICE_ACCESS, client, other,
                              DixManageAccess);
                if (rc == Success) {
                    rc = _XkbSetMapChecks(client, other, stuff, tmp, FALSE);
                    if (rc != Success)
                        return rc;
                }
            }
        }
    } else {
        DeviceIntPtr other;

        for (other = inputInfo.devices; other; other = other->next) {
            if (other != dev && GetMaster(other, MASTER_KEYBOARD) != dev &&
                (other != master || dev != master->lastSlave))
                continue;

            rc = _XkbSetMapChecks(client, other, stuff, tmp, FALSE);
            if (rc != Success)
                return rc;
        }
    }

    /* We know now that we will succeed with the SetMap. In theory anyway. */
    rc = _XkbSetMap(client, dev, stuff, tmp);
    if (rc != Success)
        return rc;

    if (stuff->deviceSpec == XkbUseCoreKbd) {
        DeviceIntPtr other;

        for (other = inputInfo.devices; other; other = other->next) {
            if ((other != dev) && other->key && !IsMaster(other) &&
                GetMaster(other, MASTER_KEYBOARD) == dev) {
                rc = XaceHook(XACE_DEVICE_ACCESS, client, other,
                              DixManageAccess);
                if (rc == Success)
                    _XkbSetMap(client, other, stuff, tmp);
                /* ignore rc. if the SetMap failed although the check above
                   reported true there isn't much we can do. we still need to
                   set all other devices, hoping that at least they stay in
                   sync. */
            }
        }
    } else {
        DeviceIntPtr other;

        for (other = inputInfo.devices; other; other = other->next) {
            if (other != dev && GetMaster(other, MASTER_KEYBOARD) != dev &&
                (other != master || dev != master->lastSlave))
                continue;

            _XkbSetMap(client, other, stuff, tmp); //ignore rc
        }
    }

    return Success;
}