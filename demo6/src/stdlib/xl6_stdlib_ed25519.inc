/* Immutable Ed25519 keys; distinct pure/context/prehash entry points. */
static const char xl6StdResidentxl6CryptoEd25519GenerateV1[] =
    "extern \"xl6CryptoEd25519GenerateV1\" func __xl_std_crypto_ed25519KeyPair() as Ed25519Key\n";
static const char xl6StdResidentxl6CryptoEd25519SignTextV1[] =
    "extern \"xl6CryptoEd25519SignTextV1\" @pure @nonblocking func __xl_std_crypto_ed25519Sign(bytes seed, string message, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoEd25519VerifyTextV1[] =
    "extern \"xl6CryptoEd25519VerifyTextV1\" @pure @nonblocking func __xl_std_crypto_ed25519Verify(bytes publicKey, string message, bytes signature, int size = -1) as bool\n";
static const char xl6StdResidentxl6CryptoEd25519VerifyContextTextV1[] =
    "extern \"xl6CryptoEd25519VerifyContextTextV1\" @pure @nonblocking func __xl_std_crypto_ed25519VerifyContext(bytes publicKey, string message, bytes signature, bytes context, int size = -1) as bool\n";
static const char xl6StdResidentxl6CryptoEd25519SignBytesV1[] =
    "extern \"xl6CryptoEd25519SignBytesV1\" @pure @nonblocking func __xl_std_crypto_ed25519Sign(bytes seed, bytes message, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoEd25519VerifyBytesV1[] =
    "extern \"xl6CryptoEd25519VerifyBytesV1\" @pure @nonblocking func __xl_std_crypto_ed25519Verify(bytes publicKey, bytes message, bytes signature, int size = -1) as bool\n";
static const char xl6StdResidentxl6CryptoEd25519VerifyContextBytesV1[] =
    "extern \"xl6CryptoEd25519VerifyContextBytesV1\" @pure @nonblocking func __xl_std_crypto_ed25519VerifyContext(bytes publicKey, bytes message, bytes signature, bytes context, int size = -1) as bool\n";
static const char xl6StdResidentxl6CryptoEd25519SignBufferV1[] =
    "extern \"xl6CryptoEd25519SignBufferV1\" @nonblocking func __xl_std_crypto_ed25519SignBuffer(bytes seed, Buffer message, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoEd25519PublicFromSeedV1[] =
    "extern \"xl6CryptoEd25519PublicFromSeedV1\" @pure @nonblocking func __xl_std_crypto_ed25519PublicKey(bytes seed) as bytes\n";
static const char xl6StdResidentxl6CryptoEd25519VerifyPrehashV1[] =
    "extern \"xl6CryptoEd25519VerifyPrehashV1\" @pure @nonblocking func __xl_std_crypto_ed25519VerifyPrehash(bytes publicKey, bytes digest, bytes signature, bytes context = b\"\") as bool\n";
static const char xl6StdResidentxl6CryptoEd25519VerifyBufferV1[] =
    "extern \"xl6CryptoEd25519VerifyBufferV1\" @nonblocking func __xl_std_crypto_ed25519VerifyBuffer(bytes publicKey, Buffer message, bytes signature, int size = -1) as bool\n";
static const char xl6StdResidentxl6CryptoEd25519VerifyContextBufferV1[] =
    "extern \"xl6CryptoEd25519VerifyContextBufferV1\" @nonblocking func __xl_std_crypto_ed25519VerifyContextBuffer(bytes publicKey, Buffer message, bytes signature, bytes context, int size = -1) as bool\n";
const char* xl6StdlibEd25519ClassSource =
    "class Ed25519Key\n"
    "    private native point handle\n"
    "    static extern \"xl6CryptoEd25519FromPkcs8V1\" @nonblocking func fromPkcs8(bytes encoded) as Ed25519Key\n"
    "    extern \"xl6CryptoEd25519ToPkcs8V1\" @pure @nonblocking func toPkcs8(bool includePublic = false) as bytes\n"
    "    static extern \"xl6CryptoEd25519FromPkcs8PemV1\" @nonblocking func fromPkcs8Pem(string encoded) as Ed25519Key\n"
    "    extern \"xl6CryptoEd25519ToPkcs8PemV1\" @pure @nonblocking func toPkcs8Pem(bool includePublic = false) as string\n"
    "    extern \"xl6CryptoEd25519PublicSpkiV1\" @pure @nonblocking func publicSpki() as bytes\n"
    "    extern \"xl6CryptoEd25519PublicSpkiPemV1\" @pure @nonblocking func publicSpkiPem() as string\n"
    "    extern \"xl6CryptoEd25519CreateV1\" @nonblocking create(bytes seed)\n"
    "    extern \"xl6CryptoEd25519DestroyV1\" @no_raise @nonblocking destroy()\n"
    "    extern \"xl6CryptoEd25519PublicV1\" @pure @nonblocking get publicKey() as bytes\n"
    "    extern \"xl6CryptoEd25519ExportSeedV1\" @pure @nonblocking func exportSeed() as bytes\n"
    "    extern \"xl6CryptoEd25519KeySignTextV1\" @pure @nonblocking func sign(string message, int size = -1) as bytes\n"
    "    extern \"xl6CryptoEd25519KeySignContextTextV1\" @pure @nonblocking func signContext(string message, bytes context, int size = -1) as bytes\n"
    "    extern \"xl6CryptoEd25519KeySignBytesV1\" @pure @nonblocking func sign(bytes message, int size = -1) as bytes\n"
    "    extern \"xl6CryptoEd25519KeySignBytesSpanV1\" @pure @nonblocking func sign(cbytes message, int size = -1) as bytes\n"
    "    extern \"xl6CryptoEd25519KeySignContextBytesV1\" @pure @nonblocking func signContext(bytes message, bytes context, int size = -1) as bytes\n"
    "    extern \"xl6CryptoEd25519KeySignBufferV1\" @nonblocking func signBuffer(Buffer message, int size = -1) as bytes\n"
    "    extern \"xl6CryptoEd25519KeySignContextBufferV1\" @nonblocking func signContextBuffer(Buffer message, bytes context, int size = -1) as bytes\n"
    "    extern \"xl6CryptoEd25519KeySignPrehashV1\" @pure @nonblocking func signPrehash(bytes digest, bytes context = b\"\") as bytes\n"
    "end\n"
;
