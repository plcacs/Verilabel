psutil_net_if_addrs(PyObject* self, PyObject* args) {
    struct ifaddrs *ifaddr, *ifa;
    int family;

    PyObject *py_retlist = PyList_New(0);
    PyObject *py_tuple = NULL;
    PyObject *py_address = NULL;
    PyObject *py_netmask = NULL;
    PyObject *py_broadcast = NULL;
    PyObject *py_ptp = NULL;

    if (py_retlist == NULL)
        return NULL;
    if (getifaddrs(&ifaddr) == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        goto error;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr)
            continue;
        family = ifa->ifa_addr->sa_family;
        py_address = psutil_convert_ipaddr(ifa->ifa_addr, family);
        // If the primary address can't be determined just skip it.
        // I've never seen this happen on Linux but I did on FreeBSD.
        if (py_address == Py_None)
            continue;
        if (py_address == NULL)
            goto error;
        py_netmask = psutil_convert_ipaddr(ifa->ifa_netmask, family);
        if (py_netmask == NULL)
            goto error;

        if (ifa->ifa_flags & IFF_BROADCAST) {
            py_broadcast = psutil_convert_ipaddr(ifa->ifa_broadaddr, family);
            Py_INCREF(Py_None);
            py_ptp = Py_None;
        }
        else if (ifa->ifa_flags & IFF_POINTOPOINT) {
            py_ptp = psutil_convert_ipaddr(ifa->ifa_dstaddr, family);
            Py_INCREF(Py_None);
            py_broadcast = Py_None;
        }
        else {
            Py_INCREF(Py_None);
            Py_INCREF(Py_None);
            py_broadcast = Py_None;
            py_ptp = Py_None;
        }

        if ((py_broadcast == NULL) || (py_ptp == NULL))
            goto error;
        py_tuple = Py_BuildValue(
            "(siOOOO)",
            ifa->ifa_name,
            family,
            py_address,
            py_netmask,
            py_broadcast,
            py_ptp
        );

        if (! py_tuple)
            goto error;
        if (PyList_Append(py_retlist, py_tuple))
            goto error;
        Py_DECREF(py_tuple);
        Py_DECREF(py_address);
        Py_DECREF(py_netmask);
        Py_DECREF(py_broadcast);
        Py_DECREF(py_ptp);
    }

    freeifaddrs(ifaddr);
    return py_retlist;

error:
    if (ifaddr != NULL)
        freeifaddrs(ifaddr);
    Py_DECREF(py_retlist);
    Py_XDECREF(py_tuple);
    Py_XDECREF(py_address);
    Py_XDECREF(py_netmask);
    Py_XDECREF(py_broadcast);
    Py_XDECREF(py_ptp);
    return NULL;
}