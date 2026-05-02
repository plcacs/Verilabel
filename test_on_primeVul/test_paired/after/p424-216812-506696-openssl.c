int setup_tests(void)
{
    ADD_ALL_TESTS(call_run_cert, OSSL_NELEM(name_fns));
    ADD_TEST(test_GENERAL_NAME_cmp);
    return 1;
}