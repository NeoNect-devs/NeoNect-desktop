// tests/main_test.cpp
#include <QCoreApplication>
#include <QtTest>
#include <iostream>
#include <cstdio>
#include "test_crypto.h"
#include "test_backend.h"
#include "test_storage.h"
#include "test_models.h"


#include "test_transport.h"
#include "test_xeddsa.h"
#include "test_x3dh_step3.h"
#include "test_x3dh.h"
#include "test_doubleratchet.h"
#include "test_aead.h"
#include "test_wirecodec.h"
#include "test_secure_storage.h"
#include "test_step9_relay.h"
#include "test_session_manager.h"
#include "test_messaging_core.h"
#include "test_offline_queue.h"

int main(int argc, char *argv[]) {
    // Disable stdout buffering
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    QCoreApplication app(argc, argv);
    app.setOrganizationName("NeoNect");
    app.setApplicationName("NeoNect");
    
    if (argc > 1 && QString(argv[1]) == "TestStep9Relay") {
        NeoNect::Tests::TestStep9Relay tsr;
        int fakeArgc = 1;
        return QTest::qExec(&tsr, fakeArgc, argv);
    }
    
    if (argc > 1 && QString(argv[1]) == "TestSessionManager") {
        TestSessionManager tsm;
        int fakeArgc = 1;
        return QTest::qExec(&tsm, fakeArgc, argv);
    }
    
    if (argc > 1 && QString(argv[1]) == "TestMessagingCore") {
        TestMessagingCore tmc;
        int fakeArgc = 1;
        return QTest::qExec(&tmc, fakeArgc, argv);
    }
    
    if (argc > 1 && QString(argv[1]) == "TestOfflineQueue") {
        TestOfflineQueue toq;
        int fakeArgc = 1;
        return QTest::qExec(&toq, fakeArgc, argv);
    }

    app.setOrganizationName("NeoNect");
    app.setApplicationName("NeoNect");
    int status = 0;

    std::cout << "\n==========================================" << std::endl;
    std::cout << "  RUNNING NEONECT DESKTOP TEST SUITES" << std::endl;
    std::cout << "==========================================\n" << std::endl;

    {
        TestCrypto tc;
        int res = QTest::qExec(&tc);
        std::cout << "[TestCrypto Result]: " << (res == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= res;
    }
    {
        TestBackend tb;
        int res = QTest::qExec(&tb);
        std::cout << "[TestBackend Result]: " << (res == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= res;
    }
    {
        TestStorage ts;
        int res = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "-o" << "storage_log.txt,txt");
        if (res != 0) {
            QFile f("storage_log.txt");
            if (f.open(QIODevice::ReadOnly)) {
                std::cout << f.readAll().toStdString() << std::endl;
            }
        }
        std::cout << "[TestStorage Result]: " << (res == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= res;
    }
    {
        TestModels tm;
        int res = QTest::qExec(&tm, QStringList() << "NeoNectTests" << "-o" << "models_log.txt,txt");
        if (res != 0) {
            QFile f("models_log.txt");
            if (f.open(QIODevice::ReadOnly)) {
                std::cout << f.readAll().toStdString() << std::endl;
            }
        }
        std::cout << "[TestModels Result]: " << (res == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= res;
    }
    

    
    {
        NeoNect::Transport::TestTransport tt;
        int res = QTest::qExec(&tt);
        std::cout << "[TestTransport Result]: " << (res == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= res;
    }
    {
        TestXEdDSA txd;
        int res = QTest::qExec(&txd);
        std::cout << "[TestXEdDSA Result]: " << (res == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= res;
    }
    {
        TestX3DHStep3 tx3;
        int res = QTest::qExec(&tx3);
        std::cout << "[TestX3DHStep3 Result]: " << (res == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= res;
    }
    {
        TestX3DH tx4;
        int res = QTest::qExec(&tx4);
        std::cout << "[TestX3DH Result]: " << (res == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= res;
    }
    {
        TestDoubleRatchet tdr;
        int res = QTest::qExec(&tdr);
        std::cout << "[TestDoubleRatchet Result]: " << (res == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= res;
    }
    {
        TestAEAD taead;
        int res = QTest::qExec(&taead);
        std::cout << "[TestAEAD Result]: " << (res == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= res;
    }
    {
        TestWireCodec twc;
        int res = QTest::qExec(&twc);
        std::cout << "[TestWireCodec Result]: " << (res == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= res;
    }
    {
        TestSecureStorage tss;
        int res = QTest::qExec(&tss);
        std::cout << "[TestSecureStorage Result]: " << (res == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= res;
    }
    std::cout << "\n==========================================" << std::endl;
    std::cout << (status == 0 ? "  ALL NEONECT TESTS PASSED SUCCESSFULLY! [100%]" : "  SOME TESTS FAILED!") << std::endl;
    std::cout << "==========================================\n" << std::endl;

    return status;
}
