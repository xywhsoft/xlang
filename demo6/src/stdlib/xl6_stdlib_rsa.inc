/* Immutable RSA keys and exact hash/salt policies. No algorithm bodies. */
static const char xl6StdResidentxl6CryptoRsaPublicRawV1[] =
    "extern \"xl6CryptoRsaPublicRawV1\" @pure @nonblocking func __xl_std_cryptoAdvanced_rsaPublic(RsaPublicKey key, bytes input) as bytes\n";
static const char xl6StdResidentxl6CryptoRsaPrivateRawV1[] =
    "extern \"xl6CryptoRsaPrivateRawV1\" func __xl_std_cryptoAdvanced_rsaPrivate(RsaPrivateKey key, bytes input) as bytes\n";
const char* xl6StdlibRsaHashSource =
    "enum RsaHash as int\n    sha1Legacy = 1\n    sha224 = 2\n    sha256 = 3\n    sha384 = 4\n    sha512 = 5\nend\n";
const char* xl6StdlibRsaPrivatePartSource =
    "enum RsaPrivatePart as int\n    privateExponent = 0\n    prime1 = 1\n    prime2 = 2\n    exponent1 = 3\n    exponent2 = 4\n    coefficient = 5\nend\n";
const char* xl6StdlibRsaPublicClassSource =
    "class RsaPublicKey\n"
    "    private native point handle\n"
    "    extern \"xl6CryptoRsaPublicCreateV1\" @nonblocking create(bytes modulus, bytes exponent)\n"
    "    extern \"xl6CryptoRsaPublicDestroyV1\" @no_raise @nonblocking destroy()\n"
    "    extern \"xl6CryptoRsaPublicModulusV1\" @pure @nonblocking get modulus() as bytes\n"
    "    extern \"xl6CryptoRsaPublicExponentV1\" @pure @nonblocking get exponent() as bytes\n"
    "    extern \"xl6CryptoRsaPublicSizeV1\" @pure @nonblocking get modulusSize() as int\n"
    "    extern \"xl6CryptoRsaPublicBitsV1\" @pure @nonblocking get modulusBits() as int\n"
    "    extern \"xl6CryptoRsaVerifyPssHashV1\" @pure @nonblocking func verifyPssHash(bytes digest, bytes signature, RsaHash algorithm = RsaHash.sha256, int saltLength = -1) as bool\n"
    "    extern \"xl6CryptoRsaVerifyPssHashWithMaskV1\" @pure @nonblocking func verifyPssHashWithMask(bytes digest, bytes signature, RsaHash algorithm, RsaHash maskAlgorithm, int saltLength = -1) as bool\n"
    "    extern \"xl6CryptoRsaVerifyPssHashAnySaltV1\" @pure @nonblocking func verifyPssHashAnySalt(bytes digest, bytes signature, RsaHash algorithm = RsaHash.sha256) as bool\n"
    "    extern \"xl6CryptoRsaVerifyPssHashAnySaltWithMaskV1\" @pure @nonblocking func verifyPssHashAnySaltWithMask(bytes digest, bytes signature, RsaHash algorithm, RsaHash maskAlgorithm) as bool\n"
    "    extern \"xl6CryptoRsaVerifyPkcs1HashV1\" @pure @nonblocking func verifyPkcs1Hash(bytes digest, bytes signature, RsaHash algorithm = RsaHash.sha256) as bool\n"
    "end\n";
const char* xl6StdlibRsaPrivateClassSource =
    "class RsaPrivateKey\n"
    "    private native point handle\n"
    "    extern \"xl6CryptoRsaPrivateCreateV1\" create(bytes modulus, bytes exponent, bytes privateExponent)\n"
    "    extern \"xl6CryptoRsaPrivateCreateCrtV1\" create(bytes modulus, bytes exponent, bytes privateExponent, bytes prime1, bytes prime2, bytes exponent1, bytes exponent2, bytes coefficient)\n"
    "    extern \"xl6CryptoRsaPrivateDestroyV1\" @no_raise @nonblocking destroy()\n"
    "    extern \"xl6CryptoRsaPrivatePublicV1\" @pure @nonblocking get publicKey() as RsaPublicKey\n"
    "    extern \"xl6CryptoRsaPrivateExportPartV1\" @pure @nonblocking func exportPart(RsaPrivatePart part) as bytes\n"
    "    extern \"xl6CryptoRsaPrivateHasCrtV1\" @pure @nonblocking get hasCrt() as bool\n"
    "    extern \"xl6CryptoRsaSignPssHashV1\" func signPssHash(bytes digest, RsaHash algorithm = RsaHash.sha256) as bytes\n"
    "    extern \"xl6CryptoRsaSignPssHashWithMaskV1\" func signPssHashWithMask(bytes digest, RsaHash algorithm, RsaHash maskAlgorithm) as bytes\n"
    "    extern \"xl6CryptoRsaSignPssHashSaltV1\" func signPssHashSalt(bytes digest, bytes salt, RsaHash algorithm = RsaHash.sha256) as bytes\n"
    "    extern \"xl6CryptoRsaSignPssHashSaltWithMaskV1\" func signPssHashSaltWithMask(bytes digest, bytes salt, RsaHash algorithm, RsaHash maskAlgorithm) as bytes\n"
    "    extern \"xl6CryptoRsaSignPkcs1HashV1\" func signPkcs1Hash(bytes digest, RsaHash algorithm = RsaHash.sha256) as bytes\n"
    "end\n";
