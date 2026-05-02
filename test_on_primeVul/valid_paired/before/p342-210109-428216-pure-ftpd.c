int init_aliases(void)
{
    FILE *fp;
    char alias[MAXALIASLEN + 1U];
    char dir[PATH_MAX + 1U];

    if ((fp = fopen(ALIASES_FILE, "r")) == NULL) {
        return 0;
    }
    while (fgets(alias, sizeof alias, fp) != NULL) {
        if (*alias == '#' || *alias == '\n' || *alias == 0) {
            continue;
        }
        {
            char * const z = alias + strlen(alias) - 1U;

            if (*z != '\n') {
                goto bad;
            }
            *z = 0;
        }
        do {
            if (fgets(dir, sizeof dir, fp) == NULL || *dir == 0) {
                goto bad;
            }
            {
                char * const z = dir + strlen(dir) - 1U;

                if (*z == '\n') {
                    *z = 0;
                }
            }
        } while (*dir == '#' || *dir == 0);
        if (head == NULL) {
            if ((head = tail = malloc(sizeof *head)) == NULL ||
                (tail->alias = strdup(alias)) == NULL ||
                (tail->dir = strdup(dir)) == NULL) {
                die_mem();
            }
            tail->next = NULL;
        } else {
            DirAlias *curr;

            if ((curr = malloc(sizeof *curr)) == NULL ||
                (curr->alias = strdup(alias)) == NULL ||
                (curr->dir = strdup(dir)) == NULL) {
                die_mem();
            }
            tail->next = curr;
            tail = curr;
        }
    }
    fclose(fp);
    aliases_up++;

    return 0;

    bad:
    fclose(fp);
    logfile(LOG_ERR, MSG_ALIASES_BROKEN_FILE " [" ALIASES_FILE "]");

    return -1;
}