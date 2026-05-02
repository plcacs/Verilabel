resolve_superior_type(const char *name, const char *mod_name, const struct lys_module *module,
                      const struct lys_node *parent, struct lys_tpdf **ret)
{
    int i, j;
    struct lys_tpdf *tpdf, *match;
    int tpdf_size;

    if (!mod_name) {
        /* no prefix, try built-in types */
        for (i = 1; i < LY_DATA_TYPE_COUNT; i++) {
            if (!strcmp(ly_types[i]->name, name)) {
                if (ret) {
                    *ret = ly_types[i];
                }
                return EXIT_SUCCESS;
            }
        }
    } else {
        if (!strcmp(mod_name, module->name)) {
            /* prefix refers to the current module, ignore it */
            mod_name = NULL;
        }
    }

    if (!mod_name && parent) {
        /* search in local typedefs */
        while (parent) {
            switch (parent->nodetype) {
            case LYS_CONTAINER:
                tpdf_size = ((struct lys_node_container *)parent)->tpdf_size;
                tpdf = ((struct lys_node_container *)parent)->tpdf;
                break;

            case LYS_LIST:
                tpdf_size = ((struct lys_node_list *)parent)->tpdf_size;
                tpdf = ((struct lys_node_list *)parent)->tpdf;
                break;

            case LYS_GROUPING:
                tpdf_size = ((struct lys_node_grp *)parent)->tpdf_size;
                tpdf = ((struct lys_node_grp *)parent)->tpdf;
                break;

            case LYS_RPC:
            case LYS_ACTION:
                tpdf_size = ((struct lys_node_rpc_action *)parent)->tpdf_size;
                tpdf = ((struct lys_node_rpc_action *)parent)->tpdf;
                break;

            case LYS_NOTIF:
                tpdf_size = ((struct lys_node_notif *)parent)->tpdf_size;
                tpdf = ((struct lys_node_notif *)parent)->tpdf;
                break;

            case LYS_INPUT:
            case LYS_OUTPUT:
                tpdf_size = ((struct lys_node_inout *)parent)->tpdf_size;
                tpdf = ((struct lys_node_inout *)parent)->tpdf;
                break;

            default:
                parent = lys_parent(parent);
                continue;
            }

            for (i = 0; i < tpdf_size; i++) {
                if (!strcmp(tpdf[i].name, name) && tpdf[i].type.base > 0) {
                    match = &tpdf[i];
                    goto check_leafref;
                }
            }

            parent = lys_parent(parent);
        }
    } else {
        /* get module where to search */
        module = lyp_get_module(module, NULL, 0, mod_name, 0, 0);
        if (!module) {
            return -1;
        }
    }

    /* search in top level typedefs */
    for (i = 0; i < module->tpdf_size; i++) {
        if (!strcmp(module->tpdf[i].name, name) && module->tpdf[i].type.base > 0) {
            match = &module->tpdf[i];
            goto check_leafref;
        }
    }

    /* search in submodules */
    for (i = 0; i < module->inc_size && module->inc[i].submodule; i++) {
        for (j = 0; j < module->inc[i].submodule->tpdf_size; j++) {
            if (!strcmp(module->inc[i].submodule->tpdf[j].name, name) && module->inc[i].submodule->tpdf[j].type.base > 0) {
                match = &module->inc[i].submodule->tpdf[j];
                goto check_leafref;
            }
        }
    }

    return EXIT_FAILURE;

check_leafref:
    if (ret) {
        *ret = match;
    }
    if (match->type.base == LY_TYPE_LEAFREF) {
        while (!match->type.info.lref.path) {
            match = match->type.der;
            assert(match);
        }
    }
    return EXIT_SUCCESS;
}