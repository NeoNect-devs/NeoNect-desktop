#include "test_aead.h"
#include "../src/crypto/doubleratchet/AEAD.h"
#include "../src/crypto/OpenSSLBackend.h"
#include <QByteArray>
#include <openssl/evp.h>
#include <memory>

using namespace NeoNect::Crypto;
using namespace NeoNect::Crypto::DoubleRatchet;

class TestAEAD::Private {
public:
    std::unique_ptr<ICryptoBackend> backend;
    std::unique_ptr<IAEAD> aead;

    QByteArray generateRandom(size_t size) {
        return backend->RandomBytes(size);
    }
};

void TestAEAD::initTestCase() {
    d = new Private();
    d->backend = std::make_unique<OpenSSLBackend>();
    d->aead = std::make_unique<AEAD>(d->backend.get());
}

void TestAEAD::cleanupTestCase() {
    delete d;
}

void TestAEAD::testBasicEncryptDecrypt() {
    QByteArray mk = d->generateRandom(32);
    QByteArray pt = "hello world";
    QByteArray aad = "aad data";

    ByteView mkView{reinterpret_cast<const uint8_t*>(mk.constData()), (size_t)mk.size()};
    ByteView ptView{reinterpret_cast<const uint8_t*>(pt.constData()), (size_t)pt.size()};
    ByteView aadView{reinterpret_cast<const uint8_t*>(aad.constData()), (size_t)aad.size()};

    auto result = d->aead->encrypt(mkView, ptView, aadView);
    QVERIFY(result.success);
    QVERIFY(result.ciphertext.size() == pt.size());
    QVERIFY(result.tag.data.size() == 16);

    ByteView ctView{reinterpret_cast<const uint8_t*>(result.ciphertext.constData()), (size_t)result.ciphertext.size()};
    ByteView tagView{reinterpret_cast<const uint8_t*>(result.tag.data.constData()), (size_t)result.tag.data.size()};

    auto decResult = d->aead->decrypt(mkView, ctView, tagView, aadView);
    QVERIFY(decResult.has_value());
    QCOMPARE(decResult.value(), pt);
}

void TestAEAD::testEmptyPlaintext() {
    QByteArray mk = d->generateRandom(32);
    QByteArray pt;
    QByteArray aad = "aad data";

    ByteView mkView{reinterpret_cast<const uint8_t*>(mk.constData()), (size_t)mk.size()};
    ByteView ptView{nullptr, 0};
    ByteView aadView{reinterpret_cast<const uint8_t*>(aad.constData()), (size_t)aad.size()};

    auto result = d->aead->encrypt(mkView, ptView, aadView);
    QVERIFY(result.success);
    QVERIFY(result.ciphertext.isEmpty());
    QVERIFY(result.tag.data.size() == 16);

    ByteView ctView{nullptr, 0};
    ByteView tagView{reinterpret_cast<const uint8_t*>(result.tag.data.constData()), (size_t)result.tag.data.size()};

    auto decResult = d->aead->decrypt(mkView, ctView, tagView, aadView);
    QVERIFY(decResult.has_value());
    QVERIFY(decResult.value().isEmpty());
}

void TestAEAD::testLargePlaintext() {
    QByteArray mk = d->generateRandom(32);
    QByteArray pt = d->generateRandom(1024 * 1024); // 1 MB
    QByteArray aad = "aad data";

    ByteView mkView{reinterpret_cast<const uint8_t*>(mk.constData()), (size_t)mk.size()};
    ByteView ptView{reinterpret_cast<const uint8_t*>(pt.constData()), (size_t)pt.size()};
    ByteView aadView{reinterpret_cast<const uint8_t*>(aad.constData()), (size_t)aad.size()};

    auto result = d->aead->encrypt(mkView, ptView, aadView);
    QVERIFY(result.success);
    QCOMPARE(result.ciphertext.size(), pt.size());
    QVERIFY(result.tag.data.size() == 16);

    ByteView ctView{reinterpret_cast<const uint8_t*>(result.ciphertext.constData()), (size_t)result.ciphertext.size()};
    ByteView tagView{reinterpret_cast<const uint8_t*>(result.tag.data.constData()), (size_t)result.tag.data.size()};

    auto decResult = d->aead->decrypt(mkView, ctView, tagView, aadView);
    QVERIFY(decResult.has_value());
    QCOMPARE(decResult.value(), pt);
}

void TestAEAD::testEmptyAAD() {
    QByteArray mk = d->generateRandom(32);
    QByteArray pt = "test";
    
    ByteView mkView{reinterpret_cast<const uint8_t*>(mk.constData()), (size_t)mk.size()};
    ByteView ptView{reinterpret_cast<const uint8_t*>(pt.constData()), (size_t)pt.size()};
    ByteView aadView{nullptr, 0};

    auto result = d->aead->encrypt(mkView, ptView, aadView);
    QVERIFY(result.success);
    
    ByteView ctView{reinterpret_cast<const uint8_t*>(result.ciphertext.constData()), (size_t)result.ciphertext.size()};
    ByteView tagView{reinterpret_cast<const uint8_t*>(result.tag.data.constData()), (size_t)result.tag.data.size()};

    auto decResult = d->aead->decrypt(mkView, ctView, tagView, aadView);
    QVERIFY(decResult.has_value());
    QCOMPARE(decResult.value(), pt);
}

void TestAEAD::testNonEmptyBinaryAAD() {
    QByteArray mk = d->generateRandom(32);
    QByteArray pt = "test";
    QByteArray aad;
    aad.append('\x00').append('\xFF').append('\xAA');
    
    ByteView mkView{reinterpret_cast<const uint8_t*>(mk.constData()), (size_t)mk.size()};
    ByteView ptView{reinterpret_cast<const uint8_t*>(pt.constData()), (size_t)pt.size()};
    ByteView aadView{reinterpret_cast<const uint8_t*>(aad.constData()), (size_t)aad.size()};

    auto result = d->aead->encrypt(mkView, ptView, aadView);
    QVERIFY(result.success);
    
    ByteView ctView{reinterpret_cast<const uint8_t*>(result.ciphertext.constData()), (size_t)result.ciphertext.size()};
    ByteView tagView{reinterpret_cast<const uint8_t*>(result.tag.data.constData()), (size_t)result.tag.data.size()};

    auto decResult = d->aead->decrypt(mkView, ctView, tagView, aadView);
    QVERIFY(decResult.has_value());
    QCOMPARE(decResult.value(), pt);
}

void TestAEAD::testUtf8Plaintext() {
    QByteArray mk = d->generateRandom(32);
    QString ptStr = QString::fromUtf8("こんにちは"); // "Hello" in Japanese
    QByteArray pt = ptStr.toUtf8();
    
    ByteView mkView{reinterpret_cast<const uint8_t*>(mk.constData()), (size_t)mk.size()};
    ByteView ptView{reinterpret_cast<const uint8_t*>(pt.constData()), (size_t)pt.size()};
    ByteView aadView{nullptr, 0};

    auto result = d->aead->encrypt(mkView, ptView, aadView);
    QVERIFY(result.success);
    
    ByteView ctView{reinterpret_cast<const uint8_t*>(result.ciphertext.constData()), (size_t)result.ciphertext.size()};
    ByteView tagView{reinterpret_cast<const uint8_t*>(result.tag.data.constData()), (size_t)result.tag.data.size()};

    auto decResult = d->aead->decrypt(mkView, ctView, tagView, aadView);
    QVERIFY(decResult.has_value());
    QCOMPARE(decResult.value(), pt);
}

void TestAEAD::testModifiedCiphertext() {
    QByteArray mk = d->generateRandom(32);
    QByteArray pt = "hello world";
    ByteView mkView{reinterpret_cast<const uint8_t*>(mk.constData()), (size_t)mk.size()};
    ByteView ptView{reinterpret_cast<const uint8_t*>(pt.constData()), (size_t)pt.size()};
    ByteView aadView{nullptr, 0};

    auto result = d->aead->encrypt(mkView, ptView, aadView);
    QVERIFY(result.success);
    
    result.ciphertext[0] = result.ciphertext[0] ^ 0x01; // flip a bit

    ByteView ctView{reinterpret_cast<const uint8_t*>(result.ciphertext.constData()), (size_t)result.ciphertext.size()};
    ByteView tagView{reinterpret_cast<const uint8_t*>(result.tag.data.constData()), (size_t)result.tag.data.size()};

    auto decResult = d->aead->decrypt(mkView, ctView, tagView, aadView);
    QVERIFY(!decResult.has_value());
}

void TestAEAD::testModifiedTag() {
    QByteArray mk = d->generateRandom(32);
    QByteArray pt = "hello world";
    ByteView mkView{reinterpret_cast<const uint8_t*>(mk.constData()), (size_t)mk.size()};
    ByteView ptView{reinterpret_cast<const uint8_t*>(pt.constData()), (size_t)pt.size()};
    ByteView aadView{nullptr, 0};

    auto result = d->aead->encrypt(mkView, ptView, aadView);
    QVERIFY(result.success);
    
    result.tag.data[0] = result.tag.data[0] ^ 0x01; // flip a bit

    ByteView ctView{reinterpret_cast<const uint8_t*>(result.ciphertext.constData()), (size_t)result.ciphertext.size()};
    ByteView tagView{reinterpret_cast<const uint8_t*>(result.tag.data.constData()), (size_t)result.tag.data.size()};

    auto decResult = d->aead->decrypt(mkView, ctView, tagView, aadView);
    QVERIFY(!decResult.has_value());
}

void TestAEAD::testModifiedAAD() {
    QByteArray mk = d->generateRandom(32);
    QByteArray pt = "hello world";
    QByteArray aad = "valid aad";
    ByteView mkView{reinterpret_cast<const uint8_t*>(mk.constData()), (size_t)mk.size()};
    ByteView ptView{reinterpret_cast<const uint8_t*>(pt.constData()), (size_t)pt.size()};
    ByteView aadView{reinterpret_cast<const uint8_t*>(aad.constData()), (size_t)aad.size()};

    auto result = d->aead->encrypt(mkView, ptView, aadView);
    QVERIFY(result.success);
    
    QByteArray modifiedAad = "invalid aad";
    ByteView modifiedAadView{reinterpret_cast<const uint8_t*>(modifiedAad.constData()), (size_t)modifiedAad.size()};

    ByteView ctView{reinterpret_cast<const uint8_t*>(result.ciphertext.constData()), (size_t)result.ciphertext.size()};
    ByteView tagView{reinterpret_cast<const uint8_t*>(result.tag.data.constData()), (size_t)result.tag.data.size()};

    auto decResult = d->aead->decrypt(mkView, ctView, tagView, modifiedAadView);
    QVERIFY(!decResult.has_value());
}

void TestAEAD::testWrongMessageKey() {
    QByteArray mk = d->generateRandom(32);
    QByteArray wrongMk = d->generateRandom(32);
    QByteArray pt = "hello world";
    ByteView mkView{reinterpret_cast<const uint8_t*>(mk.constData()), (size_t)mk.size()};
    ByteView ptView{reinterpret_cast<const uint8_t*>(pt.constData()), (size_t)pt.size()};
    ByteView aadView{nullptr, 0};

    auto result = d->aead->encrypt(mkView, ptView, aadView);
    QVERIFY(result.success);
    
    ByteView wrongMkView{reinterpret_cast<const uint8_t*>(wrongMk.constData()), (size_t)wrongMk.size()};
    ByteView ctView{reinterpret_cast<const uint8_t*>(result.ciphertext.constData()), (size_t)result.ciphertext.size()};
    ByteView tagView{reinterpret_cast<const uint8_t*>(result.tag.data.constData()), (size_t)result.tag.data.size()};

    auto decResult = d->aead->decrypt(wrongMkView, ctView, tagView, aadView);
    QVERIFY(!decResult.has_value());
}

void TestAEAD::testTruncatedCiphertext() {
    QByteArray mk = d->generateRandom(32);
    QByteArray pt = "hello world";
    ByteView mkView{reinterpret_cast<const uint8_t*>(mk.constData()), (size_t)mk.size()};
    ByteView ptView{reinterpret_cast<const uint8_t*>(pt.constData()), (size_t)pt.size()};
    ByteView aadView{nullptr, 0};

    auto result = d->aead->encrypt(mkView, ptView, aadView);
    QVERIFY(result.success);
    
    QByteArray truncatedCt = result.ciphertext.left(result.ciphertext.size() - 1);
    ByteView ctView{reinterpret_cast<const uint8_t*>(truncatedCt.constData()), (size_t)truncatedCt.size()};
    ByteView tagView{reinterpret_cast<const uint8_t*>(result.tag.data.constData()), (size_t)result.tag.data.size()};

    auto decResult = d->aead->decrypt(mkView, ctView, tagView, aadView);
    QVERIFY(!decResult.has_value());
}

void TestAEAD::testMalformedTagLength() {
    QByteArray mk = d->generateRandom(32);
    QByteArray pt = "hello world";
    ByteView mkView{reinterpret_cast<const uint8_t*>(mk.constData()), (size_t)mk.size()};
    ByteView ptView{reinterpret_cast<const uint8_t*>(pt.constData()), (size_t)pt.size()};
    ByteView aadView{nullptr, 0};

    auto result = d->aead->encrypt(mkView, ptView, aadView);
    QVERIFY(result.success);
    
    QByteArray malformedTag = result.tag.data.left(15);
    ByteView ctView{reinterpret_cast<const uint8_t*>(result.ciphertext.constData()), (size_t)result.ciphertext.size()};
    ByteView tagView{reinterpret_cast<const uint8_t*>(malformedTag.constData()), (size_t)malformedTag.size()};

    auto decResult = d->aead->decrypt(mkView, ctView, tagView, aadView);
    QVERIFY(!decResult.has_value());
}

void TestAEAD::testDeterministicOutput() {
    QByteArray mk = d->generateRandom(32);
    QByteArray pt = "deterministic";
    QByteArray aad = "aad";

    ByteView mkView{reinterpret_cast<const uint8_t*>(mk.constData()), (size_t)mk.size()};
    ByteView ptView{reinterpret_cast<const uint8_t*>(pt.constData()), (size_t)pt.size()};
    ByteView aadView{reinterpret_cast<const uint8_t*>(aad.constData()), (size_t)aad.size()};

    auto result1 = d->aead->encrypt(mkView, ptView, aadView);
    auto result2 = d->aead->encrypt(mkView, ptView, aadView);

    QCOMPARE(result1.ciphertext, result2.ciphertext);
    QCOMPARE(result1.tag.data, result2.tag.data);
}

void TestAEAD::testChangingMessageKey() {
    QByteArray mk1 = d->generateRandom(32);
    QByteArray mk2 = d->generateRandom(32);
    QByteArray pt = "hello";

    ByteView mk1View{reinterpret_cast<const uint8_t*>(mk1.constData()), (size_t)mk1.size()};
    ByteView mk2View{reinterpret_cast<const uint8_t*>(mk2.constData()), (size_t)mk2.size()};
    ByteView ptView{reinterpret_cast<const uint8_t*>(pt.constData()), (size_t)pt.size()};
    ByteView aadView{nullptr, 0};

    auto result1 = d->aead->encrypt(mk1View, ptView, aadView);
    auto result2 = d->aead->encrypt(mk2View, ptView, aadView);

    QVERIFY(result1.ciphertext != result2.ciphertext || result1.tag.data != result2.tag.data);
}

void TestAEAD::testChangingAAD() {
    QByteArray mk = d->generateRandom(32);
    QByteArray pt = "hello";
    QByteArray aad1 = "aad1";
    QByteArray aad2 = "aad2";

    ByteView mkView{reinterpret_cast<const uint8_t*>(mk.constData()), (size_t)mk.size()};
    ByteView ptView{reinterpret_cast<const uint8_t*>(pt.constData()), (size_t)pt.size()};
    ByteView aad1View{reinterpret_cast<const uint8_t*>(aad1.constData()), (size_t)aad1.size()};
    ByteView aad2View{reinterpret_cast<const uint8_t*>(aad2.constData()), (size_t)aad2.size()};

    auto result1 = d->aead->encrypt(mkView, ptView, aad1View);
    auto result2 = d->aead->encrypt(mkView, ptView, aad2View);

    // Ciphertext might be the same if AES-GCM is used and PT is short? 
    // Actually, AAD only affects the tag in GCM, not the ciphertext.
    QCOMPARE(result1.ciphertext, result2.ciphertext);
    QVERIFY(result1.tag.data != result2.tag.data);
}

void TestAEAD::testChangingPlaintext() {
    QByteArray mk = d->generateRandom(32);
    QByteArray pt1 = "hello 1";
    QByteArray pt2 = "hello 2";

    ByteView mkView{reinterpret_cast<const uint8_t*>(mk.constData()), (size_t)mk.size()};
    ByteView pt1View{reinterpret_cast<const uint8_t*>(pt1.constData()), (size_t)pt1.size()};
    ByteView pt2View{reinterpret_cast<const uint8_t*>(pt2.constData()), (size_t)pt2.size()};
    ByteView aadView{nullptr, 0};

    auto result1 = d->aead->encrypt(mkView, pt1View, aadView);
    auto result2 = d->aead->encrypt(mkView, pt2View, aadView);

    QVERIFY(result1.ciphertext != result2.ciphertext);
    QVERIFY(result1.tag.data != result2.tag.data);
}

void TestAEAD::testFixedVector() {
    // Vector generated independently using python cryptography library
    // This is a NeoNect deterministic test vector, not an official Signal vector.
    
    QByteArray mk(32, 0x01);
    QByteArray pt = "Hello, Double Ratchet!";
    QByteArray aad = "AD_Data_123";
    
    // Expected values from the Python script:
    QByteArray expectedKey = QByteArray::fromHex("6d8aca2bece70d9dfaef588cbe9d78639aa062d9530b37ae4a2fb2bd72bdb664");
    QByteArray expectedNonce = QByteArray::fromHex("ea2c0de2e7785032e0e24c48");
    QByteArray expectedCt = QByteArray::fromHex("1437c469a17f6d28f1383be706664de9f671cc315e4d");
    QByteArray expectedTag = QByteArray::fromHex("ddbe9292b57cf137b6d979654832091f");
    
    // Verify derived key and nonce directly as required
    QByteArray okm = d->backend->HkdfSha256(mk, QByteArray(32, '\0'), "NeoNectAEADv1", 44);
    QVERIFY(okm.size() == 44);
    QCOMPARE(okm.left(32), expectedKey);
    QCOMPARE(okm.mid(32, 12), expectedNonce);

    ByteView mkView{reinterpret_cast<const uint8_t*>(mk.constData()), (size_t)mk.size()};
    ByteView ptView{reinterpret_cast<const uint8_t*>(pt.constData()), (size_t)pt.size()};
    ByteView aadView{reinterpret_cast<const uint8_t*>(aad.constData()), (size_t)aad.size()};

    auto result = d->aead->encrypt(mkView, ptView, aadView);
    
    QVERIFY(result.success);
    QCOMPARE(result.ciphertext, expectedCt);
    QCOMPARE(result.tag.data, expectedTag);
    
    // Also test decryption of the expected vector
    ByteView ctView{reinterpret_cast<const uint8_t*>(expectedCt.constData()), (size_t)expectedCt.size()};
    ByteView tagView{reinterpret_cast<const uint8_t*>(expectedTag.constData()), (size_t)expectedTag.size()};
    
    auto decResult = d->aead->decrypt(mkView, ctView, tagView, aadView);
    QVERIFY(decResult.has_value());
    QCOMPARE(decResult.value(), pt);
}

void TestAEAD::testOpenSSLInteroperability() {
    // NeoNect AEAD encrypt -> independent OpenSSL decrypt
    QByteArray mk(32, 0x02);
    QByteArray pt = "cross check OpenSSL!";
    QByteArray aad = "some_context";

    ByteView mkView{reinterpret_cast<const uint8_t*>(mk.constData()), (size_t)mk.size()};
    ByteView ptView{reinterpret_cast<const uint8_t*>(pt.constData()), (size_t)pt.size()};
    ByteView aadView{reinterpret_cast<const uint8_t*>(aad.constData()), (size_t)aad.size()};

    auto neoEncryptResult = d->aead->encrypt(mkView, ptView, aadView);
    QVERIFY(neoEncryptResult.success);

    // Derive key/nonce using direct OpenSSL HKDF EVP APIs (independent of NeoNect ICryptoBackend)
    EVP_KDF *kdf = EVP_KDF_fetch(nullptr, "HKDF", nullptr);
    EVP_KDF_CTX *kctx = EVP_KDF_CTX_new(kdf);
    
    OSSL_PARAM params[5];
    const char *md = "SHA256";
    const char *info = "NeoNectAEADv1";
    QByteArray salt(32, 0);

    params[0] = OSSL_PARAM_construct_utf8_string("digest", (char *)md, 0);
    params[1] = OSSL_PARAM_construct_octet_string("key", mk.data(), mk.size());
    params[2] = OSSL_PARAM_construct_octet_string("info", (char *)info, strlen(info));
    params[3] = OSSL_PARAM_construct_octet_string("salt", salt.data(), salt.size());
    params[4] = OSSL_PARAM_construct_end();

    QByteArray okm(44, 0);
    EVP_KDF_derive(kctx, reinterpret_cast<unsigned char*>(okm.data()), okm.size(), params);
    
    EVP_KDF_CTX_free(kctx);
    EVP_KDF_free(kdf);

    QByteArray indKey = okm.left(32);
    QByteArray indNonce = okm.mid(32, 12);

    // Independent decrypt
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
    EVP_DecryptInit_ex(ctx, nullptr, nullptr, reinterpret_cast<const unsigned char*>(indKey.constData()), reinterpret_cast<const unsigned char*>(indNonce.constData()));

    int outLen = 0;
    EVP_DecryptUpdate(ctx, nullptr, &outLen, reinterpret_cast<const unsigned char*>(aad.constData()), aad.size());

    QByteArray decPt(pt.size(), 0);
    EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(decPt.data()), &outLen, reinterpret_cast<const unsigned char*>(neoEncryptResult.ciphertext.constData()), neoEncryptResult.ciphertext.size());

    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, neoEncryptResult.tag.data.data());
    int ret = EVP_DecryptFinal_ex(ctx, nullptr, &outLen);
    EVP_CIPHER_CTX_free(ctx);

    QVERIFY(ret > 0);
    QCOMPARE(decPt, pt);

    // Independent encrypt -> NeoNect AEAD decrypt
    EVP_CIPHER_CTX *ctxEnc = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctxEnc, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctxEnc, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
    EVP_EncryptInit_ex(ctxEnc, nullptr, nullptr, reinterpret_cast<const unsigned char*>(indKey.constData()), reinterpret_cast<const unsigned char*>(indNonce.constData()));

    EVP_EncryptUpdate(ctxEnc, nullptr, &outLen, reinterpret_cast<const unsigned char*>(aad.constData()), aad.size());

    QByteArray encCt(pt.size(), 0);
    EVP_EncryptUpdate(ctxEnc, reinterpret_cast<unsigned char*>(encCt.data()), &outLen, reinterpret_cast<const unsigned char*>(pt.constData()), pt.size());
    EVP_EncryptFinal_ex(ctxEnc, nullptr, &outLen);

    QByteArray encTag(16, 0);
    EVP_CIPHER_CTX_ctrl(ctxEnc, EVP_CTRL_GCM_GET_TAG, 16, encTag.data());
    EVP_CIPHER_CTX_free(ctxEnc);

    ByteView indCtView{reinterpret_cast<const uint8_t*>(encCt.constData()), (size_t)encCt.size()};
    ByteView indTagView{reinterpret_cast<const uint8_t*>(encTag.constData()), (size_t)encTag.size()};

    auto neoDecryptResult = d->aead->decrypt(mkView, indCtView, indTagView, aadView);
    QVERIFY(neoDecryptResult.has_value());
    QCOMPARE(neoDecryptResult.value(), pt);
}
