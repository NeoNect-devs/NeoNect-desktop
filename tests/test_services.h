// tests/test_services.h
#pragma once
#include <QObject>

class TestServices : public QObject {
    Q_OBJECT

private slots:
    void testAuthServiceFlow();
    void testDeviceServiceFlow();
    void testRelayServiceFlowAndDeduplication();
    void testAllFriendsUsesBackendAuthority();
    void testEmptyBackendFriendsProducesEmptyModel();
    void testStaleLocalFriendsDoNotOverrideBackend();
    void testAddFriendSendsRealRequest();
    void testPendingFriendIsNotAccepted();
    void testAcceptedFriendAppears();
    void testRejectedFriendDoesNotAppear();
    void testRemovedFriendDisappears();
    void testNoHardcodedFriendFallback();
    void testNetworkManagerFacadeIntegration();
    void testTwoClientChatExchange();
    void testTwoClientFriendRequestFlow();
    void testTwoClientFriendRequestRejectFlow();
    void testTwoClientMediaRequestApprovalFlow();
    void testBookmarkConnectFlow();
    void testNotificationManagerFlow();
    void testCiphertextDoesNotContainPlaintext();
    void testRelayServiceTamperedMessageRejection();
    void testCallbackCannotReachDestroyedService();
    void testRequestCancellationOnServiceDestruction();
    void testPhase10ABackendProtocolCompliance();
    void testConnectivityAndOnlinePresence();
    void testOpenConversationsActivityOrdering();
    void testOpenConversationsUnreadCountBadge();
    void testSeenReceiptsAndUpdateCheckmark();
    void testSelfDirectMessageAndSavedMessagesFlow();
    void testRetryMessageFlow();
    void testFunctionalOnlineIdleDndInvisibleStates();
    void testRealtimeChatPresenceExchange();
};
