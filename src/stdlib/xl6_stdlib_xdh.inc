/* Immutable managed XDH keys; raw scalar multiplication is explicitly advanced. */
static const char xl6StdResidentxl6CryptoX25519GenerateV1[] =
    "extern \"xl6CryptoX25519GenerateV1\" func __xl_std_crypto_x25519KeyPair() as X25519Key\n";
static const char xl6StdResidentxl6CryptoX25519RawV1[] =
    "extern \"xl6CryptoX25519RawV1\" @pure @nonblocking func __xl_std_cryptoAdvanced_x25519(bytes scalar, bytes point) as bytes\n";
static const char xl6StdResidentxl6CryptoX448GenerateV1[] =
    "extern \"xl6CryptoX448GenerateV1\" func __xl_std_crypto_x448KeyPair() as X448Key\n";
static const char xl6StdResidentxl6CryptoX448RawV1[] =
    "extern \"xl6CryptoX448RawV1\" @pure @nonblocking func __xl_std_cryptoAdvanced_x448(bytes scalar, bytes point) as bytes\n";
const char* xl6StdlibXdhClassSource =
    "class X25519Key\n"
    "    private native point handle\n"
    "    static extern \"xl6CryptoX25519FromPkcs8V1\" @nonblocking func fromPkcs8(bytes encoded) as X25519Key\n"
    "    extern \"xl6CryptoX25519ToPkcs8V1\" @pure @nonblocking func toPkcs8(bool includePublic = false) as bytes\n"
    "    static extern \"xl6CryptoX25519FromPkcs8PemV1\" @nonblocking func fromPkcs8Pem(string encoded) as X25519Key\n"
    "    extern \"xl6CryptoX25519ToPkcs8PemV1\" @pure @nonblocking func toPkcs8Pem(bool includePublic = false) as string\n"
    "    extern \"xl6CryptoX25519PublicSpkiV1\" @pure @nonblocking func publicSpki() as bytes\n"
    "    extern \"xl6CryptoX25519PublicSpkiPemV1\" @pure @nonblocking func publicSpkiPem() as string\n"
    "    extern \"xl6CryptoX25519CreateV1\" @nonblocking create(bytes privateKey)\n"
    "    extern \"xl6CryptoX25519DestroyV1\" @no_raise @nonblocking destroy()\n"
    "    extern \"xl6CryptoX25519PublicV1\" @pure @nonblocking get publicKey() as bytes\n"
    "    extern \"xl6CryptoX25519ExportPrivateV1\" @pure @nonblocking func exportPrivate() as bytes\n"
    "    extern \"xl6CryptoX25519SharedV1\" @pure @nonblocking func sharedSecret(bytes peerPublicKey) as bytes\n"
    "end\n"
    "class X448Key\n"
    "    private native point handle\n"
    "    static extern \"xl6CryptoX448FromPkcs8V1\" @nonblocking func fromPkcs8(bytes encoded) as X448Key\n"
    "    extern \"xl6CryptoX448ToPkcs8V1\" @pure @nonblocking func toPkcs8(bool includePublic = false) as bytes\n"
    "    static extern \"xl6CryptoX448FromPkcs8PemV1\" @nonblocking func fromPkcs8Pem(string encoded) as X448Key\n"
    "    extern \"xl6CryptoX448ToPkcs8PemV1\" @pure @nonblocking func toPkcs8Pem(bool includePublic = false) as string\n"
    "    extern \"xl6CryptoX448PublicSpkiV1\" @pure @nonblocking func publicSpki() as bytes\n"
    "    extern \"xl6CryptoX448PublicSpkiPemV1\" @pure @nonblocking func publicSpkiPem() as string\n"
    "    extern \"xl6CryptoX448CreateV1\" @nonblocking create(bytes privateKey)\n"
    "    extern \"xl6CryptoX448DestroyV1\" @no_raise @nonblocking destroy()\n"
    "    extern \"xl6CryptoX448PublicV1\" @pure @nonblocking get publicKey() as bytes\n"
    "    extern \"xl6CryptoX448ExportPrivateV1\" @pure @nonblocking func exportPrivate() as bytes\n"
    "    extern \"xl6CryptoX448SharedV1\" @pure @nonblocking func sharedSecret(bytes peerPublicKey) as bytes\n"
    "end\n"
;
