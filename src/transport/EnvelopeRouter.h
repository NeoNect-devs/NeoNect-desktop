#pragma once
#include "IIncomingEnvelopeHandler.h"
#include <memory>

namespace NeoNect {
namespace Transport {

class EnvelopeRouter : public IIncomingEnvelopeHandler {
public:
    void setSessionManagerHandler(std::shared_ptr<IIncomingEnvelopeHandler> handler) {
        m_sessionManager = std::move(handler);
    }
    
    void setFileTransferHandler(std::shared_ptr<IIncomingEnvelopeHandler> handler) {
        m_fileTransfer = std::move(handler);
    }

    NeoNect::VoidResult handleEnvelope(const QByteArray &envelope, const TransportMetadata &metadata) override {
        if (envelope.size() < 2) return NeoNect::VoidResult::fail("envelope too short");
        uint8_t type = envelope.at(1);
        if (type == 0x03) { // FILE_CHUNK
            if (m_fileTransfer) return m_fileTransfer->handleEnvelope(envelope, metadata);
            return NeoNect::VoidResult::fail("no file transfer handler");
        } else {
            if (m_sessionManager) return m_sessionManager->handleEnvelope(envelope, metadata);
            return NeoNect::VoidResult::fail("no session manager handler");
        }
    }
private:
    std::shared_ptr<IIncomingEnvelopeHandler> m_sessionManager;
    std::shared_ptr<IIncomingEnvelopeHandler> m_fileTransfer;
};

} // namespace Transport
} // namespace NeoNect
