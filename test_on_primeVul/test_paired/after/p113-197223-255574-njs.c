njs_module_path(njs_vm_t *vm, const njs_str_t *dir, njs_module_info_t *info)
{
    char        *p;
    size_t      length;
    njs_bool_t  trail;
    char        src[NJS_MAX_PATH + 1];

    trail = 0;
    length = info->name.length;

    if (dir != NULL) {
        length += dir->length;

        if (length == 0) {
            return NJS_DECLINED;
        }

        trail = (dir->start[dir->length - 1] != '/');

        if (trail) {
            length++;
        }
    }

    if (njs_slow_path(length > NJS_MAX_PATH)) {
        return NJS_ERROR;
    }

    p = &src[0];

    if (dir != NULL) {
        p = (char *) njs_cpymem(p, dir->start, dir->length);

        if (trail) {
            *p++ = '/';
        }
    }

    p = (char *) njs_cpymem(p, info->name.start, info->name.length);
    *p = '\0';

    p = realpath(&src[0], &info->path[0]);
    if (p == NULL) {
        return NJS_DECLINED;
    }

    info->fd = open(&info->path[0], O_RDONLY);
    if (info->fd < 0) {
        return NJS_DECLINED;
    }


    info->file.start = (u_char *) &info->path[0];
    info->file.length = njs_strlen(info->file.start);

    return NJS_OK;
}