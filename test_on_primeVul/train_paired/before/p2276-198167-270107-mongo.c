int linenoiseHistorySave(const char* filename) {
    FILE* fp = fopen(filename, "wt");
    if (fp == NULL) {
        return -1;
    }

    for (int j = 0; j < historyLen; ++j) {
        if (history[j][0] != '\0') {
            fprintf(fp, "%s\n", history[j]);
        }
    }
    fclose(fp);
    return 0;
}