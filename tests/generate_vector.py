import os
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

mk = bytes([1] * 32)
pt = b"Hello, Double Ratchet!"
aad = b"AD_Data_123"

hkdf = HKDF(
    algorithm=hashes.SHA256(),
    length=44,
    salt=b"\x00" * 32,
    info=b"NeoNectAEADv1"
)
okm = hkdf.derive(mk)
key = okm[:32]
nonce = okm[32:]

aesgcm = AESGCM(key)
ct_with_tag = aesgcm.encrypt(nonce, pt, aad)

ct = ct_with_tag[:-16]
tag = ct_with_tag[-16:]

def to_hex(b):
    return ", ".join([f"0x{x:02x}" for x in b])

print("MK:", to_hex(mk))
print("PT:", to_hex(pt))
print("AAD:", to_hex(aad))
print("Key:", to_hex(key))
print("Nonce:", to_hex(nonce))
print("CT:", to_hex(ct))
print("Tag:", to_hex(tag))
