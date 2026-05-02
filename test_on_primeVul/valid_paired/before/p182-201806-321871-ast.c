Sfdouble_t sh_strnum(Shell_t *shp, const char *str, char **ptr, int mode) {
    Sfdouble_t d;
    char *last;

    if (*str == 0) {
        if (ptr) *ptr = (char *)str;
        return 0;
    }
    errno = 0;
    d = number(str, &last, shp->inarith ? 0 : 10, NULL);
    if (*last) {
        if (*last != '.' || last[1] != '.') {
            d = strval(shp, str, &last, arith, mode);
            Varsubscript = true;
        }
        if (!ptr && *last && mode > 0) errormsg(SH_DICT, ERROR_exit(1), e_lexbadchar, *last, str);
    } else if (!d && *str == '-') {
        d = -0.0;
    }
    if (ptr) *ptr = last;
    return d;
}