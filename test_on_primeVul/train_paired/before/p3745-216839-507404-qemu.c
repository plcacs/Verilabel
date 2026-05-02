int main(int argc, char **argv)
{
    int fd;
    int ret;

    /* Create a temporary raw image */
    fd = mkstemp(test_image);
    g_assert(fd >= 0);
    ret = ftruncate(fd, TEST_IMAGE_SIZE);
    g_assert(ret == 0);
    close(fd);

    /* Run the tests */
    g_test_init(&argc, &argv, NULL);

    qtest_start("-machine pc -device floppy,id=floppy0");
    qtest_irq_intercept_in(global_qtest, "ioapic");
    qtest_add_func("/fdc/cmos", test_cmos);
    qtest_add_func("/fdc/no_media_on_start", test_no_media_on_start);
    qtest_add_func("/fdc/read_without_media", test_read_without_media);
    qtest_add_func("/fdc/media_change", test_media_change);
    qtest_add_func("/fdc/sense_interrupt", test_sense_interrupt);
    qtest_add_func("/fdc/relative_seek", test_relative_seek);
    qtest_add_func("/fdc/read_id", test_read_id);
    qtest_add_func("/fdc/verify", test_verify);
    qtest_add_func("/fdc/media_insert", test_media_insert);
    qtest_add_func("/fdc/read_no_dma_1", test_read_no_dma_1);
    qtest_add_func("/fdc/read_no_dma_18", test_read_no_dma_18);
    qtest_add_func("/fdc/read_no_dma_19", test_read_no_dma_19);
    qtest_add_func("/fdc/fuzz-registers", fuzz_registers);
    qtest_add_func("/fdc/fuzz/cve_2021_20196", test_cve_2021_20196);

    ret = g_test_run();

    /* Cleanup */
    qtest_end();
    unlink(test_image);

    return ret;
}