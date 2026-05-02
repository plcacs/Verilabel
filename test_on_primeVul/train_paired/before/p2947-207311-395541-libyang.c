resolve_iffeature(struct lys_iffeature *expr)
{
    int index_e = 0, index_f = 0;

    if (expr->expr) {
        return resolve_iffeature_recursive(expr, &index_e, &index_f);
    }
    return 0;
}