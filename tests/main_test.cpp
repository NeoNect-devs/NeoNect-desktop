// tests/main_test.cpp
#include <QCoreApplication>
#include <QtTest>
#include <iostream>
#include <cstdio>
#include "test_crypto.h"
#include "test_storage.h"
#include "test_models.h"
#include "test_services.h"
#include "test_messages.h"

int main(int argc, char *argv[]) {
    // Disable stdout buffering
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    QCoreApplication app(argc, argv);
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
        int res = QTest::qExec(&tm);
        std::cout << "[TestModels Result]: " << (res == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= res;
    }
    {
        TestServices ts;
        std::cout << "--> Testing testAuthServiceFlow..." << std::endl;
        int r1 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testAuthServiceFlow");
        std::cout << "    Result: " << (r1 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testDeviceServiceFlow..." << std::endl;
        int r2 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testDeviceServiceFlow");
        std::cout << "    Result: " << (r2 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testRelayServiceFlowAndDeduplication..." << std::endl;
        int r3 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testRelayServiceFlowAndDeduplication");
        std::cout << "    Result: " << (r3 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testAllFriendsUsesBackendAuthority..." << std::endl;
        int r4_1 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testAllFriendsUsesBackendAuthority");
        std::cout << "    Result: " << (r4_1 == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= r4_1;

        std::cout << "--> Testing testEmptyBackendFriendsProducesEmptyModel..." << std::endl;
        int r4_2 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testEmptyBackendFriendsProducesEmptyModel");
        std::cout << "    Result: " << (r4_2 == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= r4_2;

        std::cout << "--> Testing testStaleLocalFriendsDoNotOverrideBackend..." << std::endl;
        int r4_3 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testStaleLocalFriendsDoNotOverrideBackend");
        std::cout << "    Result: " << (r4_3 == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= r4_3;

        std::cout << "--> Testing testAddFriendSendsRealRequest..." << std::endl;
        int r4_4 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testAddFriendSendsRealRequest");
        std::cout << "    Result: " << (r4_4 == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= r4_4;

        std::cout << "--> Testing testPendingFriendIsNotAccepted..." << std::endl;
        int r4_5 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testPendingFriendIsNotAccepted");
        std::cout << "    Result: " << (r4_5 == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= r4_5;

        std::cout << "--> Testing testAcceptedFriendAppears..." << std::endl;
        int r4_6 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testAcceptedFriendAppears");
        std::cout << "    Result: " << (r4_6 == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= r4_6;

        std::cout << "--> Testing testRejectedFriendDoesNotAppear..." << std::endl;
        int r4_7 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testRejectedFriendDoesNotAppear");
        std::cout << "    Result: " << (r4_7 == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= r4_7;

        std::cout << "--> Testing testRemovedFriendDisappears..." << std::endl;
        int r4_8 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testRemovedFriendDisappears");
        std::cout << "    Result: " << (r4_8 == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= r4_8;

        std::cout << "--> Testing testNoHardcodedFriendFallback..." << std::endl;
        int r4_9 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testNoHardcodedFriendFallback");
        std::cout << "    Result: " << (r4_9 == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= r4_9;

        std::cout << "--> Testing testNetworkManagerFacadeIntegration..." << std::endl;
        int r5 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testNetworkManagerFacadeIntegration");
        std::cout << "    Result: " << (r5 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testTwoClientChatExchange..." << std::endl;
        int r6 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testTwoClientChatExchange");
        std::cout << "    Result: " << (r6 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testTwoClientFriendRequestFlow..." << std::endl;
        int r6_2 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testTwoClientFriendRequestFlow");
        std::cout << "    Result: " << (r6_2 == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= r6_2;

        std::cout << "--> Testing testTwoClientMediaRequestApprovalFlow..." << std::endl;
        int r6_3 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testTwoClientMediaRequestApprovalFlow");
        std::cout << "    Result: " << (r6_3 == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= r6_3;

        std::cout << "--> Testing testBookmarkConnectFlow..." << std::endl;
        int r7 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testBookmarkConnectFlow");
        std::cout << "    Result: " << (r7 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testNotificationManagerFlow..." << std::endl;
        int r8 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testNotificationManagerFlow");
        std::cout << "    Result: " << (r8 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testCiphertextDoesNotContainPlaintext..." << std::endl;
        int r9 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testCiphertextDoesNotContainPlaintext");
        std::cout << "    Result: " << (r9 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testRelayServiceTamperedMessageRejection..." << std::endl;
        int r10 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testRelayServiceTamperedMessageRejection");
        std::cout << "    Result: " << (r10 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testCallbackCannotReachDestroyedService..." << std::endl;
        int r11 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testCallbackCannotReachDestroyedService");
        std::cout << "    Result: " << (r11 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testRequestCancellationOnServiceDestruction..." << std::endl;
        int r12 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testRequestCancellationOnServiceDestruction");
        std::cout << "    Result: " << (r12 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testPhase10ABackendProtocolCompliance..." << std::endl;
        int r13 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testPhase10ABackendProtocolCompliance");
        std::cout << "    Result: " << (r13 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testConnectivityAndOnlinePresence..." << std::endl;
        int r14 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testConnectivityAndOnlinePresence" << "-v2");
        std::cout << "    Result: " << (r14 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testOpenConversationsActivityOrdering..." << std::endl;
        int r15 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testOpenConversationsActivityOrdering");
        std::cout << "    Result: " << (r15 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testOpenConversationsUnreadCountBadge..." << std::endl;
        int r16 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testOpenConversationsUnreadCountBadge" << "-v2");
        std::cout << "    Result: " << (r16 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testSeenReceiptsAndUpdateCheckmark..." << std::endl;
        int r17 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testSeenReceiptsAndUpdateCheckmark" << "-v2");
        std::cout << "    Result: " << (r17 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testSelfDirectMessageAndSavedMessagesFlow..." << std::endl;
        int r18 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testSelfDirectMessageAndSavedMessagesFlow" << "-o" << "test_self.txt,txt");
        std::cout << "    Result: " << (r18 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testRetryMessageFlow..." << std::endl;
        int r19 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testRetryMessageFlow" << "-o" << "test_retry.txt,txt");
        std::cout << "    Result: " << (r19 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testFunctionalOnlineIdleDndInvisibleStates..." << std::endl;
        int r20 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testFunctionalOnlineIdleDndInvisibleStates");
        std::cout << "    Result: " << (r20 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testRealtimeChatPresenceExchange..." << std::endl;
        int r21 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testRealtimeChatPresenceExchange");
        std::cout << "    Result: " << (r21 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testDisplayNameResolutionAndSync..." << std::endl;
        int r22 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testDisplayNameResolutionAndSync" << "-o" << "dn_log.txt,txt");
        if (r22 != 0) {
            QFile f("dn_log.txt");
            if (f.open(QIODevice::ReadOnly)) {
                std::cout << f.readAll().toStdString() << std::endl;
            }
        }
        std::cout << "    Result: " << (r22 == 0 ? "PASSED" : "FAILED") << std::endl;

        std::cout << "--> Testing testAvatarProcessingAndPeerSync..." << std::endl;
        int r23 = QTest::qExec(&ts, QStringList() << "NeoNectTests" << "testAvatarProcessingAndPeerSync" << "-o" << "av_log.txt,txt");
        if (r23 != 0) {
            QFile f("av_log.txt");
            if (f.open(QIODevice::ReadOnly)) {
                std::cout << f.readAll().toStdString() << std::endl;
            }
        }
        std::cout << "    Result: " << (r23 == 0 ? "PASSED" : "FAILED") << std::endl;

        status |= (r1 | r2 | r3 | 0 | r5 | r6 | r6_2 | r7 | r8 | r9 | r10 | r11 | r12 | r13 | r14 | r15 | r16 | r17 | r18 | r19 | r20 | r21 | r22 | r23);
    }

    {
        TestMessages tm;
        int res = QTest::qExec(&tm);
        std::cout << "[TestMessages Result]: " << (res == 0 ? "PASSED" : "FAILED") << std::endl;
        status |= res;
    }
    std::cout << "\n==========================================" << std::endl;
    std::cout << (status == 0 ? "  ALL NEONECT TESTS PASSED SUCCESSFULLY! [100%]" : "  SOME TESTS FAILED!") << std::endl;
    std::cout << "==========================================\n" << std::endl;

    return status;
}
