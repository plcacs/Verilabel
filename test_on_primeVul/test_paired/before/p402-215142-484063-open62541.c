setup_secureChannel(void) {
    TestingPolicy(&dummyPolicy, dummyCertificate, &fCalled, &keySizes);
    UA_SecureChannel_init(&testChannel, &UA_ConnectionConfig_default);
    UA_SecureChannel_setSecurityPolicy(&testChannel, &dummyPolicy, &dummyCertificate);

    testingConnection = createDummyConnection(65535, &sentData);
    UA_Connection_attachSecureChannel(&testingConnection, &testChannel);
    testChannel.connection = &testingConnection;

    testChannel.state = UA_SECURECHANNELSTATE_OPEN;
}