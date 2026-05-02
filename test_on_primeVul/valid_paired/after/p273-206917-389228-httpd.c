static int identity_count(void *v, const char *key, const char *val)
{
    int *count = v;
    *count += strlen(key) * 3 + strlen(val) * 3 + 2;
    return 1;
}