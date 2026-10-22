/* Immutable NIST keys; whole digests and explicit signature encodings. */
static const char xl6StdResidentxl6CryptoP256GenerateV1[] =
    "extern \"xl6CryptoP256GenerateV1\" func __xl_std_crypto_p256KeyPair() as P256Key\n";
static const char xl6StdResidentxl6CryptoP256PublicFromPrivateV1[] =
    "extern \"xl6CryptoP256PublicFromPrivateV1\" @pure @nonblocking func __xl_std_crypto_p256PublicKey(bytes privateKey) as bytes\n";
static const char xl6StdResidentxl6CryptoP256VerifyHashV1[] =
    "extern \"xl6CryptoP256VerifyHashV1\" @pure @nonblocking func __xl_std_crypto_ecdsaP256VerifyHash(bytes publicKey, bytes digest, bytes signature) as bool\n";
static const char xl6StdResidentxl6CryptoP256VerifyHashDerV1[] =
    "extern \"xl6CryptoP256VerifyHashDerV1\" @pure @nonblocking func __xl_std_crypto_ecdsaP256VerifyHashDer(bytes publicKey, bytes digest, bytes signature) as bool\n";
static const char xl6StdResidentxl6CryptoP256ValidV1[] =
    "extern \"xl6CryptoP256ValidV1\" @pure @nonblocking func __xl_std_cryptoAdvanced_p256Valid(bytes point) as bool\n";
static const char xl6StdResidentxl6CryptoP256MultiplyV1[] =
    "extern \"xl6CryptoP256MultiplyV1\" @pure @nonblocking func __xl_std_cryptoAdvanced_p256Multiply(bytes scalar, bytes point) as bytes\n";
static const char xl6StdResidentxl6CryptoP256AddV1[] =
    "extern \"xl6CryptoP256AddV1\" @pure @nonblocking func __xl_std_cryptoAdvanced_p256Add(bytes left, bytes right) as bytes\n";
static const char xl6StdResidentxl6CryptoP384GenerateV1[] =
    "extern \"xl6CryptoP384GenerateV1\" func __xl_std_crypto_p384KeyPair() as P384Key\n";
static const char xl6StdResidentxl6CryptoP384PublicFromPrivateV1[] =
    "extern \"xl6CryptoP384PublicFromPrivateV1\" @pure @nonblocking func __xl_std_crypto_p384PublicKey(bytes privateKey) as bytes\n";
static const char xl6StdResidentxl6CryptoP384VerifyHashV1[] =
    "extern \"xl6CryptoP384VerifyHashV1\" @pure @nonblocking func __xl_std_crypto_ecdsaP384VerifyHash(bytes publicKey, bytes digest, bytes signature) as bool\n";
static const char xl6StdResidentxl6CryptoP384VerifyHashDerV1[] =
    "extern \"xl6CryptoP384VerifyHashDerV1\" @pure @nonblocking func __xl_std_crypto_ecdsaP384VerifyHashDer(bytes publicKey, bytes digest, bytes signature) as bool\n";
static const char xl6StdResidentxl6CryptoP384ValidV1[] =
    "extern \"xl6CryptoP384ValidV1\" @pure @nonblocking func __xl_std_cryptoAdvanced_p384Valid(bytes point) as bool\n";
static const char xl6StdResidentxl6CryptoP384MultiplyV1[] =
    "extern \"xl6CryptoP384MultiplyV1\" @pure @nonblocking func __xl_std_cryptoAdvanced_p384Multiply(bytes scalar, bytes point) as bytes\n";
static const char xl6StdResidentxl6CryptoP384AddV1[] =
    "extern \"xl6CryptoP384AddV1\" @pure @nonblocking func __xl_std_cryptoAdvanced_p384Add(bytes left, bytes right) as bytes\n";
static const char xl6StdResidentxl6CryptoEcdsaP256ToDerV1[] =
    "extern \"xl6CryptoEcdsaP256ToDerV1\" @pure @nonblocking func __xl_std_crypto_ecdsaP256ToDer(bytes signature) as bytes\n";
static const char xl6StdResidentxl6CryptoEcdsaP256FromDerV1[] =
    "extern \"xl6CryptoEcdsaP256FromDerV1\" @pure @nonblocking func __xl_std_crypto_ecdsaP256FromDer(bytes signature) as bytes\n";
static const char xl6StdResidentxl6CryptoEcdsaP384ToDerV1[] =
    "extern \"xl6CryptoEcdsaP384ToDerV1\" @pure @nonblocking func __xl_std_crypto_ecdsaP384ToDer(bytes signature) as bytes\n";
static const char xl6StdResidentxl6CryptoEcdsaP384FromDerV1[] =
    "extern \"xl6CryptoEcdsaP384FromDerV1\" @pure @nonblocking func __xl_std_crypto_ecdsaP384FromDer(bytes signature) as bytes\n";
const char* xl6StdlibSignatureHashSource =
    "enum SignatureHash as int\n    sha256 = 3\n    sha384 = 4\n    sha512 = 5\nend\n";
const char* xl6StdlibNistClassSource =
    "class P256Key\n"
    "    private native point handle\n"
    "    extern \"xl6CryptoP256CreateV1\" @nonblocking create(bytes privateKey)\n"
    "    extern \"xl6CryptoP256DestroyV1\" @no_raise @nonblocking destroy()\n"
    "    extern \"xl6CryptoP256PublicV1\" @pure @nonblocking get publicKey() as bytes\n"
    "    extern \"xl6CryptoP256ExportPrivateV1\" @pure @nonblocking func exportPrivate() as bytes\n"
    "    extern \"xl6CryptoP256SharedV1\" @pure @nonblocking func sharedSecret(bytes peerPublicKey) as bytes\n"
    "    extern \"xl6CryptoP256SignHashV1\" @pure @nonblocking func signHash(bytes digest, SignatureHash algorithm = SignatureHash.sha256) as bytes\n"
    "    extern \"xl6CryptoP256SignHashDerV1\" @pure @nonblocking func signHashDer(bytes digest, SignatureHash algorithm = SignatureHash.sha256) as bytes\n"
    "end\n"
    "class P384Key\n"
    "    private native point handle\n"
    "    extern \"xl6CryptoP384CreateV1\" @nonblocking create(bytes privateKey)\n"
    "    extern \"xl6CryptoP384DestroyV1\" @no_raise @nonblocking destroy()\n"
    "    extern \"xl6CryptoP384PublicV1\" @pure @nonblocking get publicKey() as bytes\n"
    "    extern \"xl6CryptoP384ExportPrivateV1\" @pure @nonblocking func exportPrivate() as bytes\n"
    "    extern \"xl6CryptoP384SharedV1\" @pure @nonblocking func sharedSecret(bytes peerPublicKey) as bytes\n"
    "    extern \"xl6CryptoP384SignHashV1\" @pure @nonblocking func signHash(bytes digest, SignatureHash algorithm = SignatureHash.sha384) as bytes\n"
    "    extern \"xl6CryptoP384SignHashDerV1\" @pure @nonblocking func signHashDer(bytes digest, SignatureHash algorithm = SignatureHash.sha384) as bytes\n"
    "end\n";
