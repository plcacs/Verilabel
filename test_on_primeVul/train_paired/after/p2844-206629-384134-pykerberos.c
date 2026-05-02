static PyObject *checkPassword(PyObject *self, PyObject *args)
{
    const char *user = NULL;
    const char *pswd = NULL;
    const char *service = NULL;
    const char *default_realm = NULL;
    const int verify = 1;
    int result = 0;

    if (!PyArg_ParseTuple(args, "ssssb", &user, &pswd, &service, &default_realm, &verify))
        return NULL;

    result = authenticate_user_krb5pwd(user, pswd, service, default_realm, verify);

    if (result)
        return Py_INCREF(Py_True), Py_True;
    else
        return NULL;
}