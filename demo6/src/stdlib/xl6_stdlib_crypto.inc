/* Resident, value-facing crypto declarations. Weak hashes are explicit legacy. */
static const char xl6StdResidentxl6CryptoMd5TextV1[] =
    "extern \"xl6CryptoMd5TextV1\" @nonblocking @pure func __xl_std_cryptoLegacy_md5(string data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoMd5TextHexV1[] =
    "extern \"xl6CryptoMd5TextHexV1\" @nonblocking @pure func __xl_std_cryptoLegacy_md5Hex(string data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoMd5BytesV1[] =
    "extern \"xl6CryptoMd5BytesV1\" @nonblocking @pure func __xl_std_cryptoLegacy_md5(bytes data, int size = -1) as bytes\n"
    "extern \"xl6CryptoMd5BytesSpanV1\" @nonblocking @pure func __xl_std_cryptoLegacy_md5(cbytes data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoMd5BytesHexV1[] =
    "extern \"xl6CryptoMd5BytesHexV1\" @nonblocking @pure func __xl_std_cryptoLegacy_md5Hex(bytes data, int size = -1) as string\n"
    "extern \"xl6CryptoMd5BytesHexSpanV1\" @nonblocking @pure func __xl_std_cryptoLegacy_md5Hex(cbytes data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoMd5BufferV1[] =
    "extern \"xl6CryptoMd5BufferV1\" @nonblocking func __xl_std_cryptoLegacy_md5Buffer(Buffer data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoMd5BufferHexV1[] =
    "extern \"xl6CryptoMd5BufferHexV1\" @nonblocking func __xl_std_cryptoLegacy_md5BufferHex(Buffer data, int size = -1) as string\n";
static const char xl6StdResidentMd5Stream[] = "func __xl_std_cryptoLegacy_md5Stream() as LegacyMd5\n    return LegacyMd5()\nend\n";
static const char xl6StdResidentxl6CryptoSha1TextV1[] =
    "extern \"xl6CryptoSha1TextV1\" @nonblocking @pure func __xl_std_cryptoLegacy_sha1(string data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoSha1TextHexV1[] =
    "extern \"xl6CryptoSha1TextHexV1\" @nonblocking @pure func __xl_std_cryptoLegacy_sha1Hex(string data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoSha1BytesV1[] =
    "extern \"xl6CryptoSha1BytesV1\" @nonblocking @pure func __xl_std_cryptoLegacy_sha1(bytes data, int size = -1) as bytes\n"
    "extern \"xl6CryptoSha1BytesSpanV1\" @nonblocking @pure func __xl_std_cryptoLegacy_sha1(cbytes data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoSha1BytesHexV1[] =
    "extern \"xl6CryptoSha1BytesHexV1\" @nonblocking @pure func __xl_std_cryptoLegacy_sha1Hex(bytes data, int size = -1) as string\n"
    "extern \"xl6CryptoSha1BytesHexSpanV1\" @nonblocking @pure func __xl_std_cryptoLegacy_sha1Hex(cbytes data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoSha1BufferV1[] =
    "extern \"xl6CryptoSha1BufferV1\" @nonblocking func __xl_std_cryptoLegacy_sha1Buffer(Buffer data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoSha1BufferHexV1[] =
    "extern \"xl6CryptoSha1BufferHexV1\" @nonblocking func __xl_std_cryptoLegacy_sha1BufferHex(Buffer data, int size = -1) as string\n";
static const char xl6StdResidentSha1Stream[] = "func __xl_std_cryptoLegacy_sha1Stream() as LegacySha1\n    return LegacySha1()\nend\n";
static const char xl6StdResidentxl6CryptoSha224TextV1[] =
    "extern \"xl6CryptoSha224TextV1\" @nonblocking @pure func __xl_std_crypto_sha224(string data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoSha224TextHexV1[] =
    "extern \"xl6CryptoSha224TextHexV1\" @nonblocking @pure func __xl_std_crypto_sha224Hex(string data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoSha224BytesV1[] =
    "extern \"xl6CryptoSha224BytesV1\" @nonblocking @pure func __xl_std_crypto_sha224(bytes data, int size = -1) as bytes\n"
    "extern \"xl6CryptoSha224BytesSpanV1\" @nonblocking @pure func __xl_std_crypto_sha224(cbytes data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoSha224BytesHexV1[] =
    "extern \"xl6CryptoSha224BytesHexV1\" @nonblocking @pure func __xl_std_crypto_sha224Hex(bytes data, int size = -1) as string\n"
    "extern \"xl6CryptoSha224BytesHexSpanV1\" @nonblocking @pure func __xl_std_crypto_sha224Hex(cbytes data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoSha224BufferV1[] =
    "extern \"xl6CryptoSha224BufferV1\" @nonblocking func __xl_std_crypto_sha224Buffer(Buffer data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoSha224BufferHexV1[] =
    "extern \"xl6CryptoSha224BufferHexV1\" @nonblocking func __xl_std_crypto_sha224BufferHex(Buffer data, int size = -1) as string\n";
static const char xl6StdResidentSha224Stream[] = "func __xl_std_crypto_sha224Stream() as Sha224\n    return Sha224()\nend\n";
static const char xl6StdResidentxl6CryptoSha256TextV1[] =
    "extern \"xl6CryptoSha256TextV1\" @nonblocking @pure func __xl_std_crypto_sha256(string data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoSha256TextHexV1[] =
    "extern \"xl6CryptoSha256TextHexV1\" @nonblocking @pure func __xl_std_crypto_sha256Hex(string data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoSha256BytesV1[] =
    "extern \"xl6CryptoSha256BytesV1\" @nonblocking @pure func __xl_std_crypto_sha256(bytes data, int size = -1) as bytes\n"
    "extern \"xl6CryptoSha256BytesSpanV1\" @nonblocking @pure func __xl_std_crypto_sha256(cbytes data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoSha256BytesHexV1[] =
    "extern \"xl6CryptoSha256BytesHexV1\" @nonblocking @pure func __xl_std_crypto_sha256Hex(bytes data, int size = -1) as string\n"
    "extern \"xl6CryptoSha256BytesHexSpanV1\" @nonblocking @pure func __xl_std_crypto_sha256Hex(cbytes data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoSha256BufferV1[] =
    "extern \"xl6CryptoSha256BufferV1\" @nonblocking func __xl_std_crypto_sha256Buffer(Buffer data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoSha256BufferHexV1[] =
    "extern \"xl6CryptoSha256BufferHexV1\" @nonblocking func __xl_std_crypto_sha256BufferHex(Buffer data, int size = -1) as string\n";
static const char xl6StdResidentSha256Stream[] = "func __xl_std_crypto_sha256Stream() as Sha256\n    return Sha256()\nend\n";
static const char xl6StdResidentxl6CryptoSha384TextV1[] =
    "extern \"xl6CryptoSha384TextV1\" @nonblocking @pure func __xl_std_crypto_sha384(string data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoSha384TextHexV1[] =
    "extern \"xl6CryptoSha384TextHexV1\" @nonblocking @pure func __xl_std_crypto_sha384Hex(string data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoSha384BytesV1[] =
    "extern \"xl6CryptoSha384BytesV1\" @nonblocking @pure func __xl_std_crypto_sha384(bytes data, int size = -1) as bytes\n"
    "extern \"xl6CryptoSha384BytesSpanV1\" @nonblocking @pure func __xl_std_crypto_sha384(cbytes data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoSha384BytesHexV1[] =
    "extern \"xl6CryptoSha384BytesHexV1\" @nonblocking @pure func __xl_std_crypto_sha384Hex(bytes data, int size = -1) as string\n"
    "extern \"xl6CryptoSha384BytesHexSpanV1\" @nonblocking @pure func __xl_std_crypto_sha384Hex(cbytes data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoSha384BufferV1[] =
    "extern \"xl6CryptoSha384BufferV1\" @nonblocking func __xl_std_crypto_sha384Buffer(Buffer data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoSha384BufferHexV1[] =
    "extern \"xl6CryptoSha384BufferHexV1\" @nonblocking func __xl_std_crypto_sha384BufferHex(Buffer data, int size = -1) as string\n";
static const char xl6StdResidentSha384Stream[] = "func __xl_std_crypto_sha384Stream() as Sha384\n    return Sha384()\nend\n";
static const char xl6StdResidentxl6CryptoSha512TextV1[] =
    "extern \"xl6CryptoSha512TextV1\" @nonblocking @pure func __xl_std_crypto_sha512(string data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoSha512TextHexV1[] =
    "extern \"xl6CryptoSha512TextHexV1\" @nonblocking @pure func __xl_std_crypto_sha512Hex(string data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoSha512BytesV1[] =
    "extern \"xl6CryptoSha512BytesV1\" @nonblocking @pure func __xl_std_crypto_sha512(bytes data, int size = -1) as bytes\n"
    "extern \"xl6CryptoSha512BytesSpanV1\" @nonblocking @pure func __xl_std_crypto_sha512(cbytes data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoSha512BytesHexV1[] =
    "extern \"xl6CryptoSha512BytesHexV1\" @nonblocking @pure func __xl_std_crypto_sha512Hex(bytes data, int size = -1) as string\n"
    "extern \"xl6CryptoSha512BytesHexSpanV1\" @nonblocking @pure func __xl_std_crypto_sha512Hex(cbytes data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoSha512BufferV1[] =
    "extern \"xl6CryptoSha512BufferV1\" @nonblocking func __xl_std_crypto_sha512Buffer(Buffer data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoSha512BufferHexV1[] =
    "extern \"xl6CryptoSha512BufferHexV1\" @nonblocking func __xl_std_crypto_sha512BufferHex(Buffer data, int size = -1) as string\n";
static const char xl6StdResidentSha512Stream[] = "func __xl_std_crypto_sha512Stream() as Sha512\n    return Sha512()\nend\n";
static const char xl6StdResidentxl6CryptoSha512_256TextV1[] =
    "extern \"xl6CryptoSha512_256TextV1\" @nonblocking @pure func __xl_std_crypto_sha512_256(string data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoSha512_256TextHexV1[] =
    "extern \"xl6CryptoSha512_256TextHexV1\" @nonblocking @pure func __xl_std_crypto_sha512_256Hex(string data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoSha512_256BytesV1[] =
    "extern \"xl6CryptoSha512_256BytesV1\" @nonblocking @pure func __xl_std_crypto_sha512_256(bytes data, int size = -1) as bytes\n"
    "extern \"xl6CryptoSha512_256BytesSpanV1\" @nonblocking @pure func __xl_std_crypto_sha512_256(cbytes data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoSha512_256BytesHexV1[] =
    "extern \"xl6CryptoSha512_256BytesHexV1\" @nonblocking @pure func __xl_std_crypto_sha512_256Hex(bytes data, int size = -1) as string\n"
    "extern \"xl6CryptoSha512_256BytesHexSpanV1\" @nonblocking @pure func __xl_std_crypto_sha512_256Hex(cbytes data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoSha512_256BufferV1[] =
    "extern \"xl6CryptoSha512_256BufferV1\" @nonblocking func __xl_std_crypto_sha512_256Buffer(Buffer data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoSha512_256BufferHexV1[] =
    "extern \"xl6CryptoSha512_256BufferHexV1\" @nonblocking func __xl_std_crypto_sha512_256BufferHex(Buffer data, int size = -1) as string\n";
static const char xl6StdResidentSha512_256Stream[] = "func __xl_std_crypto_sha512_256Stream() as Sha512_256\n    return Sha512_256()\nend\n";
static const char xl6StdResidentxl6CryptoHmacSha256TextV1[] =
    "extern \"xl6CryptoHmacSha256TextV1\" @nonblocking @pure func __xl_std_crypto_hmacSha256(bytes key, string data, int size = -1) as bytes\n"
    "extern \"xl6CryptoHmacSha256TextSpanV1\" @nonblocking @pure func __xl_std_crypto_hmacSha256(cbytes key, string data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoHmacSha256TextHexV1[] =
    "extern \"xl6CryptoHmacSha256TextHexV1\" @nonblocking @pure func __xl_std_crypto_hmacSha256Hex(bytes key, string data, int size = -1) as string\n"
    "extern \"xl6CryptoHmacSha256TextHexSpanV1\" @nonblocking @pure func __xl_std_crypto_hmacSha256Hex(cbytes key, string data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoHmacSha256BytesV1[] =
    "extern \"xl6CryptoHmacSha256BytesV1\" @nonblocking @pure func __xl_std_crypto_hmacSha256(bytes key, bytes data, int size = -1) as bytes\n"
    "extern \"xl6CryptoHmacSha256BytesSpanV1\" @nonblocking @pure func __xl_std_crypto_hmacSha256(cbytes key, cbytes data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoHmacSha256BytesHexV1[] =
    "extern \"xl6CryptoHmacSha256BytesHexV1\" @nonblocking @pure func __xl_std_crypto_hmacSha256Hex(bytes key, bytes data, int size = -1) as string\n"
    "extern \"xl6CryptoHmacSha256BytesHexSpanV1\" @nonblocking @pure func __xl_std_crypto_hmacSha256Hex(cbytes key, cbytes data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoHmacSha256BufferV1[] =
    "extern \"xl6CryptoHmacSha256BufferV1\" @nonblocking func __xl_std_crypto_hmacSha256Buffer(bytes key, Buffer data, int size = -1) as bytes\n"
    "extern \"xl6CryptoHmacSha256BufferSpanV1\" @nonblocking func __xl_std_crypto_hmacSha256Buffer(cbytes key, Buffer data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoHmacSha256BufferHexV1[] =
    "extern \"xl6CryptoHmacSha256BufferHexV1\" @nonblocking func __xl_std_crypto_hmacSha256BufferHex(bytes key, Buffer data, int size = -1) as string\n"
    "extern \"xl6CryptoHmacSha256BufferHexSpanV1\" @nonblocking func __xl_std_crypto_hmacSha256BufferHex(cbytes key, Buffer data, int size = -1) as string\n";
static const char xl6StdResidentHmacSha256Stream[] = "func __xl_std_crypto_hmacSha256Stream(bytes key) as HmacSha256\n    return HmacSha256(key)\nend\n";
static const char xl6StdResidentxl6CryptoHmacSha384TextV1[] =
    "extern \"xl6CryptoHmacSha384TextV1\" @nonblocking @pure func __xl_std_crypto_hmacSha384(bytes key, string data, int size = -1) as bytes\n"
    "extern \"xl6CryptoHmacSha384TextSpanV1\" @nonblocking @pure func __xl_std_crypto_hmacSha384(cbytes key, string data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoHmacSha384TextHexV1[] =
    "extern \"xl6CryptoHmacSha384TextHexV1\" @nonblocking @pure func __xl_std_crypto_hmacSha384Hex(bytes key, string data, int size = -1) as string\n"
    "extern \"xl6CryptoHmacSha384TextHexSpanV1\" @nonblocking @pure func __xl_std_crypto_hmacSha384Hex(cbytes key, string data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoHmacSha384BytesV1[] =
    "extern \"xl6CryptoHmacSha384BytesV1\" @nonblocking @pure func __xl_std_crypto_hmacSha384(bytes key, bytes data, int size = -1) as bytes\n"
    "extern \"xl6CryptoHmacSha384BytesSpanV1\" @nonblocking @pure func __xl_std_crypto_hmacSha384(cbytes key, cbytes data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoHmacSha384BytesHexV1[] =
    "extern \"xl6CryptoHmacSha384BytesHexV1\" @nonblocking @pure func __xl_std_crypto_hmacSha384Hex(bytes key, bytes data, int size = -1) as string\n"
    "extern \"xl6CryptoHmacSha384BytesHexSpanV1\" @nonblocking @pure func __xl_std_crypto_hmacSha384Hex(cbytes key, cbytes data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoHmacSha384BufferV1[] =
    "extern \"xl6CryptoHmacSha384BufferV1\" @nonblocking func __xl_std_crypto_hmacSha384Buffer(bytes key, Buffer data, int size = -1) as bytes\n"
    "extern \"xl6CryptoHmacSha384BufferSpanV1\" @nonblocking func __xl_std_crypto_hmacSha384Buffer(cbytes key, Buffer data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoHmacSha384BufferHexV1[] =
    "extern \"xl6CryptoHmacSha384BufferHexV1\" @nonblocking func __xl_std_crypto_hmacSha384BufferHex(bytes key, Buffer data, int size = -1) as string\n"
    "extern \"xl6CryptoHmacSha384BufferHexSpanV1\" @nonblocking func __xl_std_crypto_hmacSha384BufferHex(cbytes key, Buffer data, int size = -1) as string\n";
static const char xl6StdResidentHmacSha384Stream[] = "func __xl_std_crypto_hmacSha384Stream(bytes key) as HmacSha384\n    return HmacSha384(key)\nend\n";
static const char xl6StdResidentxl6CryptoHmacSha512TextV1[] =
    "extern \"xl6CryptoHmacSha512TextV1\" @nonblocking @pure func __xl_std_crypto_hmacSha512(bytes key, string data, int size = -1) as bytes\n"
    "extern \"xl6CryptoHmacSha512TextSpanV1\" @nonblocking @pure func __xl_std_crypto_hmacSha512(cbytes key, string data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoHmacSha512TextHexV1[] =
    "extern \"xl6CryptoHmacSha512TextHexV1\" @nonblocking @pure func __xl_std_crypto_hmacSha512Hex(bytes key, string data, int size = -1) as string\n"
    "extern \"xl6CryptoHmacSha512TextHexSpanV1\" @nonblocking @pure func __xl_std_crypto_hmacSha512Hex(cbytes key, string data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoHmacSha512BytesV1[] =
    "extern \"xl6CryptoHmacSha512BytesV1\" @nonblocking @pure func __xl_std_crypto_hmacSha512(bytes key, bytes data, int size = -1) as bytes\n"
    "extern \"xl6CryptoHmacSha512BytesSpanV1\" @nonblocking @pure func __xl_std_crypto_hmacSha512(cbytes key, cbytes data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoHmacSha512BytesHexV1[] =
    "extern \"xl6CryptoHmacSha512BytesHexV1\" @nonblocking @pure func __xl_std_crypto_hmacSha512Hex(bytes key, bytes data, int size = -1) as string\n"
    "extern \"xl6CryptoHmacSha512BytesHexSpanV1\" @nonblocking @pure func __xl_std_crypto_hmacSha512Hex(cbytes key, cbytes data, int size = -1) as string\n";
static const char xl6StdResidentxl6CryptoHmacSha512BufferV1[] =
    "extern \"xl6CryptoHmacSha512BufferV1\" @nonblocking func __xl_std_crypto_hmacSha512Buffer(bytes key, Buffer data, int size = -1) as bytes\n"
    "extern \"xl6CryptoHmacSha512BufferSpanV1\" @nonblocking func __xl_std_crypto_hmacSha512Buffer(cbytes key, Buffer data, int size = -1) as bytes\n";
static const char xl6StdResidentxl6CryptoHmacSha512BufferHexV1[] =
    "extern \"xl6CryptoHmacSha512BufferHexV1\" @nonblocking func __xl_std_crypto_hmacSha512BufferHex(bytes key, Buffer data, int size = -1) as string\n"
    "extern \"xl6CryptoHmacSha512BufferHexSpanV1\" @nonblocking func __xl_std_crypto_hmacSha512BufferHex(cbytes key, Buffer data, int size = -1) as string\n";
static const char xl6StdResidentHmacSha512Stream[] = "func __xl_std_crypto_hmacSha512Stream(bytes key) as HmacSha512\n    return HmacSha512(key)\nend\n";
static const char xl6StdResidentxl6CryptoPbkdf2Sha256V1[] =
    "extern \"xl6CryptoPbkdf2Sha256V1\" @pure func __xl_std_crypto_pbkdf2Sha256(bytes password, bytes salt, int iterations, int length) as bytes\n"
    "extern \"xl6CryptoPbkdf2Sha256SpanV1\" @pure func __xl_std_crypto_pbkdf2Sha256(cbytes password, cbytes salt, int iterations, int length) as bytes\n";
static const char xl6StdResidentxl6CryptoHkdfSha256V1[] =
    "extern \"xl6CryptoHkdfSha256V1\" @pure @nonblocking func __xl_std_crypto_hkdfSha256(bytes ikm, bytes salt, bytes info, int length) as bytes\n"
    "extern \"xl6CryptoHkdfSha256SpanV1\" @pure @nonblocking func __xl_std_crypto_hkdfSha256(cbytes ikm, cbytes salt, cbytes info, int length) as bytes\n";
static const char xl6StdResidentxl6CryptoHkdfSha256ExtractV1[] =
    "extern \"xl6CryptoHkdfSha256ExtractV1\" @pure @nonblocking func __xl_std_crypto_hkdfSha256Extract(bytes ikm, bytes salt) as bytes\n"
    "extern \"xl6CryptoHkdfSha256ExtractSpanV1\" @pure @nonblocking func __xl_std_crypto_hkdfSha256Extract(cbytes ikm, cbytes salt) as bytes\n";
static const char xl6StdResidentxl6CryptoHkdfSha256ExpandV1[] =
    "extern \"xl6CryptoHkdfSha256ExpandV1\" @pure @nonblocking func __xl_std_crypto_hkdfSha256Expand(bytes prk, bytes info, int length) as bytes\n"
    "extern \"xl6CryptoHkdfSha256ExpandSpanV1\" @pure @nonblocking func __xl_std_crypto_hkdfSha256Expand(cbytes prk, cbytes info, int length) as bytes\n";
static const char xl6StdResidentxl6CryptoPbkdf2Sha384V1[] =
    "extern \"xl6CryptoPbkdf2Sha384V1\" @pure func __xl_std_crypto_pbkdf2Sha384(bytes password, bytes salt, int iterations, int length) as bytes\n"
    "extern \"xl6CryptoPbkdf2Sha384SpanV1\" @pure func __xl_std_crypto_pbkdf2Sha384(cbytes password, cbytes salt, int iterations, int length) as bytes\n";
static const char xl6StdResidentxl6CryptoHkdfSha384V1[] =
    "extern \"xl6CryptoHkdfSha384V1\" @pure @nonblocking func __xl_std_crypto_hkdfSha384(bytes ikm, bytes salt, bytes info, int length) as bytes\n"
    "extern \"xl6CryptoHkdfSha384SpanV1\" @pure @nonblocking func __xl_std_crypto_hkdfSha384(cbytes ikm, cbytes salt, cbytes info, int length) as bytes\n";
static const char xl6StdResidentxl6CryptoHkdfSha384ExtractV1[] =
    "extern \"xl6CryptoHkdfSha384ExtractV1\" @pure @nonblocking func __xl_std_crypto_hkdfSha384Extract(bytes ikm, bytes salt) as bytes\n"
    "extern \"xl6CryptoHkdfSha384ExtractSpanV1\" @pure @nonblocking func __xl_std_crypto_hkdfSha384Extract(cbytes ikm, cbytes salt) as bytes\n";
static const char xl6StdResidentxl6CryptoHkdfSha384ExpandV1[] =
    "extern \"xl6CryptoHkdfSha384ExpandV1\" @pure @nonblocking func __xl_std_crypto_hkdfSha384Expand(bytes prk, bytes info, int length) as bytes\n"
    "extern \"xl6CryptoHkdfSha384ExpandSpanV1\" @pure @nonblocking func __xl_std_crypto_hkdfSha384Expand(cbytes prk, cbytes info, int length) as bytes\n";
static const char xl6StdResidentxl6CryptoPbkdf2Sha512V1[] =
    "extern \"xl6CryptoPbkdf2Sha512V1\" @pure func __xl_std_crypto_pbkdf2Sha512(bytes password, bytes salt, int iterations, int length) as bytes\n"
    "extern \"xl6CryptoPbkdf2Sha512SpanV1\" @pure func __xl_std_crypto_pbkdf2Sha512(cbytes password, cbytes salt, int iterations, int length) as bytes\n";
static const char xl6StdResidentxl6CryptoHkdfSha512V1[] =
    "extern \"xl6CryptoHkdfSha512V1\" @pure @nonblocking func __xl_std_crypto_hkdfSha512(bytes ikm, bytes salt, bytes info, int length) as bytes\n"
    "extern \"xl6CryptoHkdfSha512SpanV1\" @pure @nonblocking func __xl_std_crypto_hkdfSha512(cbytes ikm, cbytes salt, cbytes info, int length) as bytes\n";
static const char xl6StdResidentxl6CryptoHkdfSha512ExtractV1[] =
    "extern \"xl6CryptoHkdfSha512ExtractV1\" @pure @nonblocking func __xl_std_crypto_hkdfSha512Extract(bytes ikm, bytes salt) as bytes\n"
    "extern \"xl6CryptoHkdfSha512ExtractSpanV1\" @pure @nonblocking func __xl_std_crypto_hkdfSha512Extract(cbytes ikm, cbytes salt) as bytes\n";
static const char xl6StdResidentxl6CryptoHkdfSha512ExpandV1[] =
    "extern \"xl6CryptoHkdfSha512ExpandV1\" @pure @nonblocking func __xl_std_crypto_hkdfSha512Expand(bytes prk, bytes info, int length) as bytes\n"
    "extern \"xl6CryptoHkdfSha512ExpandSpanV1\" @pure @nonblocking func __xl_std_crypto_hkdfSha512Expand(cbytes prk, cbytes info, int length) as bytes\n";
static const char xl6StdResidentxl6CryptoTimingSafeEqualV1[] =
    "extern \"xl6CryptoTimingSafeEqualV1\" @pure @nonblocking func __xl_std_crypto_timingSafeEqual(bytes left, bytes right) as bool\n"
    "extern \"xl6CryptoTimingSafeEqualSpanV1\" @pure @nonblocking func __xl_std_crypto_timingSafeEqual(cbytes left, cbytes right) as bool\n";
const char* xl6StdlibCryptoClassSource =
    "class LegacyMd5\n"
    "    private native point handle\n"
    "    extern \"xl6CryptoMd5CreateV1\" create()\n"
    "    extern \"xl6CryptoMd5DestroyV1\" destroy()\n"
    "    extern \"xl6CryptoMd5CloneV1\" func clone() as LegacyMd5\n"
    "    extern \"xl6CryptoMd5ResetV1\" func reset()\n"
    "    extern \"xl6CryptoMd5UpdateTextV1\" func update(string data, int size = -1)\n"
    "    extern \"xl6CryptoMd5UpdateBytesV1\" func update(bytes data, int size = -1)\n"
    "    extern \"xl6CryptoMd5UpdateBytesSpanV1\" func update(cbytes data, int size = -1)\n"
    "    extern \"xl6CryptoMd5UpdateBufferV1\" func updateBuffer(Buffer data, int size = -1)\n"
    "    extern \"xl6CryptoMd5FinalV1\" func final() as bytes\n"
    "    extern \"xl6CryptoMd5HexV1\" func hex() as string\n"
    "end\n"
    "class LegacySha1\n"
    "    private native point handle\n"
    "    extern \"xl6CryptoSha1CreateV1\" create()\n"
    "    extern \"xl6CryptoSha1DestroyV1\" destroy()\n"
    "    extern \"xl6CryptoSha1CloneV1\" func clone() as LegacySha1\n"
    "    extern \"xl6CryptoSha1ResetV1\" func reset()\n"
    "    extern \"xl6CryptoSha1UpdateTextV1\" func update(string data, int size = -1)\n"
    "    extern \"xl6CryptoSha1UpdateBytesV1\" func update(bytes data, int size = -1)\n"
    "    extern \"xl6CryptoSha1UpdateBytesSpanV1\" func update(cbytes data, int size = -1)\n"
    "    extern \"xl6CryptoSha1UpdateBufferV1\" func updateBuffer(Buffer data, int size = -1)\n"
    "    extern \"xl6CryptoSha1FinalV1\" func final() as bytes\n"
    "    extern \"xl6CryptoSha1HexV1\" func hex() as string\n"
    "end\n"
    "class Sha224\n"
    "    private native point handle\n"
    "    extern \"xl6CryptoSha224CreateV1\" create()\n"
    "    extern \"xl6CryptoSha224DestroyV1\" destroy()\n"
    "    extern \"xl6CryptoSha224CloneV1\" func clone() as Sha224\n"
    "    extern \"xl6CryptoSha224ResetV1\" func reset()\n"
    "    extern \"xl6CryptoSha224UpdateTextV1\" func update(string data, int size = -1)\n"
    "    extern \"xl6CryptoSha224UpdateBytesV1\" func update(bytes data, int size = -1)\n"
    "    extern \"xl6CryptoSha224UpdateBytesSpanV1\" func update(cbytes data, int size = -1)\n"
    "    extern \"xl6CryptoSha224UpdateBufferV1\" func updateBuffer(Buffer data, int size = -1)\n"
    "    extern \"xl6CryptoSha224FinalV1\" func final() as bytes\n"
    "    extern \"xl6CryptoSha224HexV1\" func hex() as string\n"
    "end\n"
    "class Sha256\n"
    "    private native point handle\n"
    "    extern \"xl6CryptoSha256CreateV1\" create()\n"
    "    extern \"xl6CryptoSha256DestroyV1\" destroy()\n"
    "    extern \"xl6CryptoSha256CloneV1\" func clone() as Sha256\n"
    "    extern \"xl6CryptoSha256ResetV1\" func reset()\n"
    "    extern \"xl6CryptoSha256UpdateTextV1\" func update(string data, int size = -1)\n"
    "    extern \"xl6CryptoSha256UpdateBytesV1\" func update(bytes data, int size = -1)\n"
    "    extern \"xl6CryptoSha256UpdateBytesSpanV1\" func update(cbytes data, int size = -1)\n"
    "    extern \"xl6CryptoSha256UpdateBufferV1\" func updateBuffer(Buffer data, int size = -1)\n"
    "    extern \"xl6CryptoSha256FinalV1\" func final() as bytes\n"
    "    extern \"xl6CryptoSha256HexV1\" func hex() as string\n"
    "end\n"
    "class Sha384\n"
    "    private native point handle\n"
    "    extern \"xl6CryptoSha384CreateV1\" create()\n"
    "    extern \"xl6CryptoSha384DestroyV1\" destroy()\n"
    "    extern \"xl6CryptoSha384CloneV1\" func clone() as Sha384\n"
    "    extern \"xl6CryptoSha384ResetV1\" func reset()\n"
    "    extern \"xl6CryptoSha384UpdateTextV1\" func update(string data, int size = -1)\n"
    "    extern \"xl6CryptoSha384UpdateBytesV1\" func update(bytes data, int size = -1)\n"
    "    extern \"xl6CryptoSha384UpdateBytesSpanV1\" func update(cbytes data, int size = -1)\n"
    "    extern \"xl6CryptoSha384UpdateBufferV1\" func updateBuffer(Buffer data, int size = -1)\n"
    "    extern \"xl6CryptoSha384FinalV1\" func final() as bytes\n"
    "    extern \"xl6CryptoSha384HexV1\" func hex() as string\n"
    "end\n"
    "class Sha512\n"
    "    private native point handle\n"
    "    extern \"xl6CryptoSha512CreateV1\" create()\n"
    "    extern \"xl6CryptoSha512DestroyV1\" destroy()\n"
    "    extern \"xl6CryptoSha512CloneV1\" func clone() as Sha512\n"
    "    extern \"xl6CryptoSha512ResetV1\" func reset()\n"
    "    extern \"xl6CryptoSha512UpdateTextV1\" func update(string data, int size = -1)\n"
    "    extern \"xl6CryptoSha512UpdateBytesV1\" func update(bytes data, int size = -1)\n"
    "    extern \"xl6CryptoSha512UpdateBytesSpanV1\" func update(cbytes data, int size = -1)\n"
    "    extern \"xl6CryptoSha512UpdateBufferV1\" func updateBuffer(Buffer data, int size = -1)\n"
    "    extern \"xl6CryptoSha512FinalV1\" func final() as bytes\n"
    "    extern \"xl6CryptoSha512HexV1\" func hex() as string\n"
    "end\n"
    "class Sha512_256\n"
    "    private native point handle\n"
    "    extern \"xl6CryptoSha512_256CreateV1\" create()\n"
    "    extern \"xl6CryptoSha512_256DestroyV1\" destroy()\n"
    "    extern \"xl6CryptoSha512_256CloneV1\" func clone() as Sha512_256\n"
    "    extern \"xl6CryptoSha512_256ResetV1\" func reset()\n"
    "    extern \"xl6CryptoSha512_256UpdateTextV1\" func update(string data, int size = -1)\n"
    "    extern \"xl6CryptoSha512_256UpdateBytesV1\" func update(bytes data, int size = -1)\n"
    "    extern \"xl6CryptoSha512_256UpdateBytesSpanV1\" func update(cbytes data, int size = -1)\n"
    "    extern \"xl6CryptoSha512_256UpdateBufferV1\" func updateBuffer(Buffer data, int size = -1)\n"
    "    extern \"xl6CryptoSha512_256FinalV1\" func final() as bytes\n"
    "    extern \"xl6CryptoSha512_256HexV1\" func hex() as string\n"
    "end\n"
    "class HmacSha256\n"
    "    private native point handle\n"
    "    extern \"xl6CryptoHmacSha256CreateV1\" create(bytes key)\n"
    "    extern \"xl6CryptoHmacSha256CreateSpanV1\" create(cbytes key)\n"
    "    extern \"xl6CryptoHmacSha256DestroyV1\" destroy()\n"
    "    extern \"xl6CryptoHmacSha256CloneV1\" func clone() as HmacSha256\n"
    "    extern \"xl6CryptoHmacSha256ResetV1\" func reset()\n"
    "    extern \"xl6CryptoHmacSha256UpdateTextV1\" func update(string data, int size = -1)\n"
    "    extern \"xl6CryptoHmacSha256UpdateBytesV1\" func update(bytes data, int size = -1)\n"
    "    extern \"xl6CryptoHmacSha256UpdateBytesSpanV1\" func update(cbytes data, int size = -1)\n"
    "    extern \"xl6CryptoHmacSha256UpdateBufferV1\" func updateBuffer(Buffer data, int size = -1)\n"
    "    extern \"xl6CryptoHmacSha256FinalV1\" func final() as bytes\n"
    "    extern \"xl6CryptoHmacSha256HexV1\" func hex() as string\n"
    "end\n"
    "class HmacSha384\n"
    "    private native point handle\n"
    "    extern \"xl6CryptoHmacSha384CreateV1\" create(bytes key)\n"
    "    extern \"xl6CryptoHmacSha384CreateSpanV1\" create(cbytes key)\n"
    "    extern \"xl6CryptoHmacSha384DestroyV1\" destroy()\n"
    "    extern \"xl6CryptoHmacSha384CloneV1\" func clone() as HmacSha384\n"
    "    extern \"xl6CryptoHmacSha384ResetV1\" func reset()\n"
    "    extern \"xl6CryptoHmacSha384UpdateTextV1\" func update(string data, int size = -1)\n"
    "    extern \"xl6CryptoHmacSha384UpdateBytesV1\" func update(bytes data, int size = -1)\n"
    "    extern \"xl6CryptoHmacSha384UpdateBytesSpanV1\" func update(cbytes data, int size = -1)\n"
    "    extern \"xl6CryptoHmacSha384UpdateBufferV1\" func updateBuffer(Buffer data, int size = -1)\n"
    "    extern \"xl6CryptoHmacSha384FinalV1\" func final() as bytes\n"
    "    extern \"xl6CryptoHmacSha384HexV1\" func hex() as string\n"
    "end\n"
    "class HmacSha512\n"
    "    private native point handle\n"
    "    extern \"xl6CryptoHmacSha512CreateV1\" create(bytes key)\n"
    "    extern \"xl6CryptoHmacSha512CreateSpanV1\" create(cbytes key)\n"
    "    extern \"xl6CryptoHmacSha512DestroyV1\" destroy()\n"
    "    extern \"xl6CryptoHmacSha512CloneV1\" func clone() as HmacSha512\n"
    "    extern \"xl6CryptoHmacSha512ResetV1\" func reset()\n"
    "    extern \"xl6CryptoHmacSha512UpdateTextV1\" func update(string data, int size = -1)\n"
    "    extern \"xl6CryptoHmacSha512UpdateBytesV1\" func update(bytes data, int size = -1)\n"
    "    extern \"xl6CryptoHmacSha512UpdateBytesSpanV1\" func update(cbytes data, int size = -1)\n"
    "    extern \"xl6CryptoHmacSha512UpdateBufferV1\" func updateBuffer(Buffer data, int size = -1)\n"
    "    extern \"xl6CryptoHmacSha512FinalV1\" func final() as bytes\n"
    "    extern \"xl6CryptoHmacSha512HexV1\" func hex() as string\n"
    "end\n"
    "class AesGcm\n"
    "    private native point handle\n"
    "    extern \"xl6CryptoAesGcmCreateV1\" create(bytes key)\n"
    "    extern \"xl6CryptoAesGcmCreateSpanV1\" create(cbytes key)\n"
    "    extern \"xl6CryptoAesGcmDestroyV1\" destroy()\n"
    "    extern \"xl6CryptoAesGcmSealV1\" func seal(bytes data, bytes nonce, bytes aad = b\"\", int size = -1, int aadSize = -1) as bytes\n"
    "    extern \"xl6CryptoAesGcmSealTextV1\" func seal(string data, bytes nonce, bytes aad = b\"\", int size = -1, int aadSize = -1) as bytes\n"
    "    extern \"xl6CryptoAesGcmOpenV1\" func open(bytes data, bytes nonce, bytes aad = b\"\", int size = -1, int aadSize = -1) as bytes\n"
    "end\n"
    "class ChaCha20Poly1305\n"
    "    private native point handle\n"
    "    extern \"xl6CryptoChaChaAeadCreateV1\" create(bytes key)\n"
    "    extern \"xl6CryptoChaChaAeadCreateSpanV1\" create(cbytes key)\n"
    "    extern \"xl6CryptoChaChaAeadDestroyV1\" destroy()\n"
    "    extern \"xl6CryptoChaChaAeadSealV1\" func seal(bytes data, bytes nonce, bytes aad = b\"\", int size = -1, int aadSize = -1) as bytes\n"
    "    extern \"xl6CryptoChaChaAeadSealTextV1\" func seal(string data, bytes nonce, bytes aad = b\"\", int size = -1, int aadSize = -1) as bytes\n"
    "    extern \"xl6CryptoChaChaAeadOpenV1\" func open(bytes data, bytes nonce, bytes aad = b\"\", int size = -1, int aadSize = -1) as bytes\n"
    "end\n"
;
