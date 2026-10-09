#pragma once

#include <QObject>
#include <QtTest>

class TestFileTransfer : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void testReq01_SenderSpoolDecryptRecipientKeyReencrypt();
    void testReq02_Exact16ByteBinaryTransferId();
    void testReq03_RealLocalDeviceIdIsUsed();
    void testReq04_WrongDeviceAckRejected();
    void testReq05_DuplicateChunkAckRoutedCorrectly();
    void testReq06_NonFinalChunkWrongLengthRejected();
    void testReq07_FinalChunkWrongLengthRejected();
    void testReq08_OffsetOverflowRejected();
    void testReq09_RetryTimerActuallyFires();
    void testReq10_TargetedNackRetriesOnlyRequestedChunks();
    void testReq11_SenderSpoolDurabilityFailurePreventsFileStart();
    void testReq12_ReceiverDurabilityFailurePreventsAck();
    void testReq13_RestartCanDecryptSenderSpoolChunkNCorrectly();
    void testReq14_RestartCanReconstructReceiverSpoolChunkNCorrectly();
    void testReq15_MultiDeviceDerivedKeysRemainDistinct();
    void testReq16_EndToEndSingleSmallFileTransfer();
    void testReq17_EndToEndMultiChunkFileTransfer();
    void testReq18_EndToEndOutOfOrderTransfer();
    void testReq19_EndToEndDuplicateChunk();
    void testReq20_EndToEndReconnectResume();
    void testReq21_RestartUsesPersistedSnapshotEvenIfCapabilitiesChange();
    void testReq22_ImageTransferIncomplete();
    void testReq23_ImageTransferSecureDecryption();
    void testReq24_ImageTransferHashMismatch();
    void testReq25_ImageTransferCorruptedCiphertext();
    void testReq26_ImageTransferMissingChunkMetadata();
    void testReq27_ImageTransferDecodingFailure();
};
