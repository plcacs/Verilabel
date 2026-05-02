ext_get_plugin(const char *name, const char *module, const char *revision)
{
    uint16_t u;

    assert(name);
    assert(module);

    for (u = 0; u < ext_plugins_count; u++) {
        if (!strcmp(name, ext_plugins[u].name) &&
                !strcmp(module, ext_plugins[u].module) &&
                (!ext_plugins[u].revision || !strcmp(revision, ext_plugins[u].revision))) {
            /* we have the match */
            return ext_plugins[u].plugin;
        }
    }

    /* plugin not found */
    return NULL;
}