testBackingParse(const void *args)
{
    const struct testBackingParseData *data = args;
    g_auto(virBuffer) buf = VIR_BUFFER_INITIALIZER;
    g_autofree char *xml = NULL;
    g_autoptr(virStorageSource) src = NULL;
    int rc;
    int erc = data->rv;

    /* expect failure return code with NULL expected data */
    if (!data->expect)
        erc = -1;

    if ((rc = virStorageSourceNewFromBackingAbsolute(data->backing, &src)) != erc) {
        fprintf(stderr, "expected return value '%d' actual '%d'\n", erc, rc);
        return -1;
    }

    if (!src)
        return 0;

    if (src && !data->expect) {
        fprintf(stderr, "parsing of backing store string '%s' should "
                        "have failed\n", data->backing);
        return -1;
    }

    if (virDomainDiskSourceFormat(&buf, src, "source", 0, false, 0, true, NULL) < 0 ||
        !(xml = virBufferContentAndReset(&buf))) {
        fprintf(stderr, "failed to format disk source xml\n");
        return -1;
    }

    if (STRNEQ(xml, data->expect)) {
        fprintf(stderr, "\n backing store string '%s'\n"
                        "expected storage source xml:\n%s\n"
                        "actual storage source xml:\n%s\n",
                        data->backing, data->expect, xml);
        return -1;
    }

    return 0;
}