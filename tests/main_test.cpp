// tests/main_test.cpp
#include <QCoreApplication>
#include <QtTest>
#include <iostream>
#include <cstdio>
#include "test_crypto.h"
#include "test_storage.h"
#include "test_models.h"
#include "test_services.h"

int main(int argc, char *argv[]) {
    // Disable stdout buffering
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    qputenv("QT_QPA_PLATFORM", "offscreen");
    QCoreApplication app(argc, argv);
    int status = 0;

    std::cout << "\n==========================================" << std::endl;
    std::cout << "  RUNNING AVILA DESKTOP TEST SUITES" << std::endl;
    std::cout << "==========================================\n" << std::endl;

    {
        TestCrypto tc;
        int res = QTest::qExec(&tc);
        std::cout << "[TestCrypto Result]: " << (res == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= res;
    }
    {
        TestStorage ts;
        int res = QTest::qExec(&ts);
        std::cout << "[TestStorage Result]: " << (res == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= res;
    }
    {
        TestModels tm;
        int res = QTest::qExec(&tm);
        std::cout << "[TestModels Result]: " << (res == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= res;
    }
    {
        TestServices ts;
        std::cout << "--> Testing testAuthServiceFlow..." << std::endl;
        int r1 = QTest::qExec(&ts, QStringList() << "AvilaTests" << "testAuthServiceFlow");
        std::cout << "    Result: " << (r1 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testDeviceServiceFlow..." << std::endl;
        int r2 = QTest::qExec(&ts, QStringList() << "AvilaTests" << "testDeviceServiceFlow");
        std::cout << "    Result: " << (r2 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testRelayServiceFlowAndDeduplication..." << std::endl;
        int r3 = QTest::qExec(&ts, QStringList() << "AvilaTests" << "testRelayServiceFlowAndDeduplication");
        std::cout << "    Result: " << (r3 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testFriendServiceFlow..." << std::endl;
        int r4 = QTest::qExec(&ts, QStringList() << "AvilaTests" << "testFriendServiceFlow");
        std::cout << "    Result: " << (r4 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testNetworkManagerFacadeIntegration..." << std::endl;
        int r5 = QTest::qExec(&ts, QStringList() << "AvilaTests" << "testNetworkManagerFacadeIntegration");
        std::cout << "    Result: " << (r5 == 0 ? "PASSED" : "FAILED") << std::endl;

        status |= (r1 | r2 | r3 | r4 | r5);
    }

    std::cout << "\n==========================================" << std::endl;
    std::cout << (status == 0 ? "  ALL AVILA TESTS PASSED SUCCESSFULLY! [100%]" : "  SOME TESTS FAILED!") << std::endl;
    std::cout << "==========================================\n" << std::endl;

    return status;
}
