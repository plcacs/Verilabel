static PyObject *checkPassword(PyObject *self, PyObject *args)
{
    const char *user = NULL;
    const char *pswd = NULL;
    const char *service = NULL;
    const char *default_realm = NULL;
    int result = 0;

    if (!PyArg_ParseTuple(args, "ssss", &user, &pswd, &service, &default_realm))
        return NULL;

    result = authenticate_user_krb5pwd(user, pswd, service, default_realm);

    if (result)
        return Py_INCREF(Py_True), Py_True;
    else
        return NULL;
}