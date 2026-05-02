int main(int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
    SpiceMarshaller *marshaller;
    SpiceMsgMainShortDataSubMarshall *msg;
    size_t len, msg_len;
    int free_res;
    uint8_t *data;
    message_destructor_t free_message;

    msg = g_new0(SpiceMsgMainShortDataSubMarshall, 1);
    msg->data = g_new(uint64_t, 2);
    msg->dummy_byte = 123;
    msg->data_size = 2;
    msg->data[0] = 0x1234567890abcdef;
    msg->data[1] = 0xfedcba0987654321;

    marshaller = spice_marshaller_new();
    spice_marshall_msg_main_ShortDataSubMarshall(marshaller, msg);
    spice_marshaller_flush(marshaller);
    data = spice_marshaller_linearize(marshaller, 0, &len, &free_res);
    g_assert_cmpint(len, ==, G_N_ELEMENTS(expected_data));
    g_assert_true(memcmp(data, expected_data, len) == 0);

    g_free(msg->data);
    g_free(msg);

    // test demarshaller
    msg = (SpiceMsgMainShortDataSubMarshall *)
        spice_parse_msg(data, data + len, SPICE_CHANNEL_MAIN, SPICE_MSG_MAIN_SHORTDATASUBMARSHALL,
                        0, &msg_len, &free_message);

    g_assert_nonnull(msg);
    g_assert_cmpint(msg->dummy_byte, ==, 123);
    g_assert_cmpint(msg->data_size, ==, 2);
    g_assert_nonnull(msg->data);
    g_assert_cmpint(msg->data[0], ==, 0x1234567890abcdef);
    g_assert_cmpint(msg->data[1], ==, 0xfedcba0987654321);

    free_message((uint8_t *) msg);

    if (free_res) {
        free(data);
    }
    spice_marshaller_reset(marshaller);

    SpiceMsgMainZeroes msg_zeroes = { 0x0102 };

    spice_marshall_msg_main_Zeroes(marshaller, &msg_zeroes);
    spice_marshaller_flush(marshaller);
    data = spice_marshaller_linearize(marshaller, 0, &len, &free_res);
    g_assert_cmpint(len, ==, 7);
    g_assert_true(memcmp(data, "\x00\x02\x01\x00\x00\x00\x00", 7) == 0);
    if (free_res) {
        free(data);
    }

    test_overflow(marshaller);

    spice_marshaller_destroy(marshaller);

    return 0;
}