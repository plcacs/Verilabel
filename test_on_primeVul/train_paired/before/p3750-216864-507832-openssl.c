static int test_x509_time(int idx)
{
    ASN1_TIME *t = NULL;
    int result, rv = 0;

    if (x509_format_tests[idx].set_string) {
        /* set-string mode */
        t = ASN1_TIME_new();
        if (t == NULL) {
            TEST_info("test_x509_time(%d) failed: internal error\n", idx);
            return 0;
        }
    }

    result = ASN1_TIME_set_string_X509(t, x509_format_tests[idx].data);
    /* time string parsing result is always checked against what's expected */
    if (!TEST_int_eq(result, x509_format_tests[idx].expected)) {
        TEST_info("test_x509_time(%d) failed: expected %d, got %d\n",
                idx, x509_format_tests[idx].expected, result);
        goto out;
    }

    /* if t is not NULL but expected_type is ignored(-1), it is an 'OK' case */
    if (t != NULL && x509_format_tests[idx].expected_type != -1) {
        if (!TEST_int_eq(t->type, x509_format_tests[idx].expected_type)) {
            TEST_info("test_x509_time(%d) failed: expected_type %d, got %d\n",
                    idx, x509_format_tests[idx].expected_type, t->type);
            goto out;
        }
    }

    /* if t is not NULL but expected_string is NULL, it is an 'OK' case too */
    if (t != NULL && x509_format_tests[idx].expected_string) {
        if (!TEST_str_eq((const char *)t->data,
                    x509_format_tests[idx].expected_string)) {
            TEST_info("test_x509_time(%d) failed: expected_string %s, got %s\n",
                    idx, x509_format_tests[idx].expected_string, t->data);
            goto out;
        }
    }

    rv = 1;
out:
    if (t != NULL)
        ASN1_TIME_free(t);
    return rv;
}