static void xl6CgenEmitCryptoRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_CRYPTO_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_CRYPTO_RUNTIME\n", out);

    xl6OutputPuts("static const char* xl6_crypto_text_or_empty(const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return text != 0 ? text : \"\";\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_crypto_empty_result(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* text = (char*)xrtMalloc(1);\n", out);
    xl6OutputPuts("    if ( text != 0 ) { text[0] = '\\0'; }\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_crypto_hex_char(char ch)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_crypto_hex_text(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t i;\n", out);
    xl6OutputPuts("    if ( text == 0 || size == 0 || (size & 1u) != 0 ) { return false; }\n", out);
    xl6OutputPuts("    for ( i = 0; i < size; ++i ) {\n", out);
    xl6OutputPuts("        if ( !xl6_crypto_hex_char(text[i]) ) { return false; }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 严格解码一段已验证的 HEX 文本。 */\n", out);
    xl6OutputPuts("static unsigned char* xl6_crypto_hex_decode(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xstrview view;\n", out);
    xl6OutputPuts("    size_t outputSize = 0u;\n", out);
    xl6OutputPuts("    view.Data = text;\n", out);
    xl6OutputPuts("    view.Size = size;\n", out);
    xl6OutputPuts("    return (unsigned char*)xrtHexDecodeNew(view, &outputSize, 0u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_crypto_decode_hex_exact(const char* text, size_t size, size_t expected, unsigned char* out)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char* decoded;\n", out);
    xl6OutputPuts("    if ( out == 0 || size != expected * 2u || !xl6_crypto_hex_text(text, size) ) { return false; }\n", out);
    xl6OutputPuts("    decoded = xl6_crypto_hex_decode(text, size);\n", out);
    xl6OutputPuts("    if ( decoded == 0 ) { return false; }\n", out);
    xl6OutputPuts("    memcpy(out, decoded, expected);\n", out);
    xl6OutputPuts("    xrtFree(decoded);\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_crypto_hex_from_bytes(const unsigned char* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* hex = xrtHexEncodeNew(data, size, 1u);\n", out);
    xl6OutputPuts("    return hex != 0 ? hex : xl6_crypto_empty_result();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_crypto_buffer_from_bytes(const void* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xbuffer* buffer;\n", out);
    xl6OutputPuts("    if ( size > 4294967295u ) { return 0; }\n", out);
    xl6OutputPuts("    buffer = xrtBufferCreate();\n", out);
    xl6OutputPuts("    if ( buffer == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( size > 0u && (data == 0 || !xrtBufferAppend(buffer, (xbytesview){ (cbytes)data, size })) ) {\n", out);
    xl6OutputPuts("        xrtBufferDestroy(buffer);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return buffer;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_crypto_table_set_hex(xvalue* table, const char* name, const unsigned char* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* hex;\n", out);
    xl6OutputPuts("    xvalue* value;\n", out);
    xl6OutputPuts("    if ( table == 0 || name == 0 ) { return; }\n", out);
    xl6OutputPuts("    hex = (char*)xrtHexEncodeNew(data, size, 1u);\n", out);
    xl6OutputPuts("    value = xl6_xrt_value_text_create(hex != 0 ? hex : \"\", 0, false);\n", out);
    xl6OutputPuts("    if ( hex != 0 ) { xrtFree(hex); }\n", out);
    xl6OutputPuts("    if ( value != 0 ) { xl6_xrt_value_object_set(table, name, 0, value, true); }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha1_hex(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[20];\n", out);
    xl6OutputPuts("    if ( !xrtSha1((const void*)xl6_crypto_text_or_empty(text), size, digest) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    return xrtHexEncodeNew(digest, sizeof(digest), 1u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha256_hex(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[32];\n", out);
    xl6OutputPuts("    if ( !xrtSha256((const void*)xl6_crypto_text_or_empty(text), size, digest) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    return xrtHexEncodeNew(digest, sizeof(digest), 1u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha384_hex(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[48];\n", out);
    xl6OutputPuts("    if ( !xrtSha384((const void*)xl6_crypto_text_or_empty(text), size, digest) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    return xrtHexEncodeNew(digest, sizeof(digest), 1u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha512_hex(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[64];\n", out);
    xl6OutputPuts("    if ( !xrtSha512((const void*)xl6_crypto_text_or_empty(text), size, digest) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    return xrtHexEncodeNew(digest, sizeof(digest), 1u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_crypto_hash_bytes(int algorithm, const void* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[64];\n", out);
    xl6OutputPuts("    size_t digestSize;\n", out);
    xl6OutputPuts("    const void* input = data != 0 ? data : \"\";\n", out);
    xl6OutputPuts("    if ( data == 0 && size != 0u ) { return 0; }\n", out);
    xl6OutputPuts("    switch ( algorithm ) {\n", out);
    xl6OutputPuts("        case 1: if ( !xrtSha1(input, size, digest) ) { return 0; } digestSize = 20u; break;\n", out);
    xl6OutputPuts("        case 2: if ( !xrtSha256(input, size, digest) ) { return 0; } digestSize = 32u; break;\n", out);
    xl6OutputPuts("        case 3: if ( !xrtSha384(input, size, digest) ) { return 0; } digestSize = 48u; break;\n", out);
    xl6OutputPuts("        case 4: if ( !xrtSha512(input, size, digest) ) { return 0; } digestSize = 64u; break;\n", out);
    xl6OutputPuts("        default: return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xl6_crypto_buffer_from_bytes(digest, digestSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha1_bytes(void* data, size_t size) { return xl6_crypto_hash_bytes(1, data, size); }\n", out);
    xl6OutputPuts("void* xl6_xrt_sha256_bytes(void* data, size_t size) { return xl6_crypto_hash_bytes(2, data, size); }\n", out);
    xl6OutputPuts("void* xl6_xrt_sha384_bytes(void* data, size_t size) { return xl6_crypto_hash_bytes(3, data, size); }\n", out);
    xl6OutputPuts("void* xl6_xrt_sha512_bytes(void* data, size_t size) { return xl6_crypto_hash_bytes(4, data, size); }\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha1_stream_create(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xsha1* ctx = (xsha1*)xrtMalloc(sizeof(xsha1));\n", out);
    xl6OutputPuts("    if ( ctx != 0 ) { xrtSha1Init(ctx); }\n", out);
    xl6OutputPuts("    return ctx;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_sha1_stream_destroy(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self != 0 ) { xrtFree(self); }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_sha1_stream_update(void* self, const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return xrtSha1Update((xsha1*)self, (const void*)xl6_crypto_text_or_empty(text), size);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha1_stream_hex(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[20];\n", out);
    xl6OutputPuts("    xsha1 copy;\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    copy = *(xsha1*)self;\n", out);
    xl6OutputPuts("    if ( !xrtSha1Final(&copy, digest) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    return xl6_crypto_hex_from_bytes(digest, sizeof(digest));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha256_stream_create(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xsha256* ctx = (xsha256*)xrtMalloc(sizeof(xsha256));\n", out);
    xl6OutputPuts("    if ( ctx != 0 ) { xrtSha256Init(ctx); }\n", out);
    xl6OutputPuts("    return ctx;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_sha256_stream_destroy(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self != 0 ) { xrtFree(self); }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_sha256_stream_update(void* self, const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return xrtSha256Update((xsha256*)self, (const void*)xl6_crypto_text_or_empty(text), size);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha256_stream_hex(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[32];\n", out);
    xl6OutputPuts("    xsha256 copy;\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    copy = *(xsha256*)self;\n", out);
    xl6OutputPuts("    if ( !xrtSha256Final(&copy, digest) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    return xl6_crypto_hex_from_bytes(digest, sizeof(digest));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha384_stream_create(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xsha384* ctx = (xsha384*)xrtMalloc(sizeof(xsha384));\n", out);
    xl6OutputPuts("    if ( ctx != 0 ) { xrtSha384Init(ctx); }\n", out);
    xl6OutputPuts("    return ctx;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_sha384_stream_destroy(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self != 0 ) { xrtFree(self); }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_sha384_stream_update(void* self, const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return xrtSha384Update((xsha384*)self, (const void*)xl6_crypto_text_or_empty(text), size);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha384_stream_hex(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[48];\n", out);
    xl6OutputPuts("    xsha384 copy;\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    copy = *(xsha384*)self;\n", out);
    xl6OutputPuts("    if ( !xrtSha384Final(&copy, digest) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    return xl6_crypto_hex_from_bytes(digest, sizeof(digest));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha512_stream_create(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xsha512* ctx = (xsha512*)xrtMalloc(sizeof(xsha512));\n", out);
    xl6OutputPuts("    if ( ctx != 0 ) { xrtSha512Init(ctx); }\n", out);
    xl6OutputPuts("    return ctx;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_sha512_stream_destroy(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self != 0 ) { xrtFree(self); }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_sha512_stream_update(void* self, const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return xrtSha512Update((xsha512*)self, (const void*)xl6_crypto_text_or_empty(text), size);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha512_stream_hex(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[64];\n", out);
    xl6OutputPuts("    xsha512 copy;\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    copy = *(xsha512*)self;\n", out);
    xl6OutputPuts("    if ( !xrtSha512Final(&copy, digest) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    return xl6_crypto_hex_from_bytes(digest, sizeof(digest));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_sha1_stream_update_bytes(void* self, void* data, size_t size)\n", out);
    xl6OutputPuts("{ if ( self == 0 || (data == 0 && size != 0u) ) { return 0; } return xrtSha1Update((xsha1*)self, data != 0 ? data : \"\", size); }\n", out);
    xl6OutputPuts("void* xl6_xrt_sha1_stream_bytes(void* self)\n", out);
    xl6OutputPuts("{ unsigned char digest[20]; if ( self == 0 || !xrtSha1Final((const xsha1*)self, digest) ) { return 0; } return xl6_crypto_buffer_from_bytes(digest, sizeof(digest)); }\n", out);
    xl6OutputPuts("int xl6_xrt_sha256_stream_update_bytes(void* self, void* data, size_t size)\n", out);
    xl6OutputPuts("{ if ( self == 0 || (data == 0 && size != 0u) ) { return 0; } return xrtSha256Update((xsha256*)self, data != 0 ? data : \"\", size); }\n", out);
    xl6OutputPuts("void* xl6_xrt_sha256_stream_bytes(void* self)\n", out);
    xl6OutputPuts("{ unsigned char digest[32]; if ( self == 0 || !xrtSha256Final((const xsha256*)self, digest) ) { return 0; } return xl6_crypto_buffer_from_bytes(digest, sizeof(digest)); }\n", out);
    xl6OutputPuts("int xl6_xrt_sha384_stream_update_bytes(void* self, void* data, size_t size)\n", out);
    xl6OutputPuts("{ if ( self == 0 || (data == 0 && size != 0u) ) { return 0; } return xrtSha384Update((xsha384*)self, data != 0 ? data : \"\", size); }\n", out);
    xl6OutputPuts("void* xl6_xrt_sha384_stream_bytes(void* self)\n", out);
    xl6OutputPuts("{ unsigned char digest[48]; if ( self == 0 || !xrtSha384Final((const xsha384*)self, digest) ) { return 0; } return xl6_crypto_buffer_from_bytes(digest, sizeof(digest)); }\n", out);
    xl6OutputPuts("int xl6_xrt_sha512_stream_update_bytes(void* self, void* data, size_t size)\n", out);
    xl6OutputPuts("{ if ( self == 0 || (data == 0 && size != 0u) ) { return 0; } return xrtSha512Update((xsha512*)self, data != 0 ? data : \"\", size); }\n", out);
    xl6OutputPuts("void* xl6_xrt_sha512_stream_bytes(void* self)\n", out);
    xl6OutputPuts("{ unsigned char digest[64]; if ( self == 0 || !xrtSha512Final((const xsha512*)self, digest) ) { return 0; } return xl6_crypto_buffer_from_bytes(digest, sizeof(digest)); }\n\n", out);

    xl6OutputPuts("static void* xl6_crypto_hmac_bytes(int algorithm, const void* key, size_t keySize, const void* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[64]; size_t digestSize;\n", out);
    xl6OutputPuts("    if ( (key == 0 && keySize != 0u) || (data == 0 && size != 0u) ) { return 0; }\n", out);
    xl6OutputPuts("    if ( algorithm == 1 ) { if ( !xrtHmacSha256(key != 0 ? key : \"\", keySize, data != 0 ? data : \"\", size, digest) ) { return 0; } digestSize = 32u; }\n", out);
    xl6OutputPuts("    else if ( algorithm == 2 ) { if ( !xrtHmacSha384(key != 0 ? key : \"\", keySize, data != 0 ? data : \"\", size, digest) ) { return 0; } digestSize = 48u; }\n", out);
    xl6OutputPuts("    else if ( algorithm == 3 ) { if ( !xrtHmacSha512(key != 0 ? key : \"\", keySize, data != 0 ? data : \"\", size, digest) ) { return 0; } digestSize = 64u; }\n", out);
    xl6OutputPuts("    else { return 0; }\n", out);
    xl6OutputPuts("    return xl6_crypto_buffer_from_bytes(digest, digestSize);\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("void* xl6_xrt_hmac_sha256_bytes(void* key, size_t keySize, void* data, size_t size) { return xl6_crypto_hmac_bytes(1, key, keySize, data, size); }\n", out);
    xl6OutputPuts("void* xl6_xrt_hmac_sha384_bytes(void* key, size_t keySize, void* data, size_t size) { return xl6_crypto_hmac_bytes(2, key, keySize, data, size); }\n", out);
    xl6OutputPuts("void* xl6_xrt_hmac_sha512_bytes(void* key, size_t keySize, void* data, size_t size) { return xl6_crypto_hmac_bytes(3, key, keySize, data, size); }\n\n", out);

    xl6OutputPuts("void* xl6_xrt_hmac_sha256_hex(const char* key, size_t keySize, const char* text, size_t textSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[32];\n", out);
    xl6OutputPuts("    if ( !xrtHmacSha256(xl6_crypto_text_or_empty(key), keySize, xl6_crypto_text_or_empty(text), textSize, digest) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    return xrtHexEncodeNew(digest, sizeof(digest), 1u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_hmac_sha384_hex(const char* key, size_t keySize, const char* text, size_t textSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[48];\n", out);
    xl6OutputPuts("    if ( !xrtHmacSha384(xl6_crypto_text_or_empty(key), keySize, xl6_crypto_text_or_empty(text), textSize, digest) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    return xrtHexEncodeNew(digest, sizeof(digest), 1u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_hmac_sha512_hex(const char* key, size_t keySize, const char* text, size_t textSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[64];\n", out);
    xl6OutputPuts("    if ( !xrtHmacSha512(xl6_crypto_text_or_empty(key), keySize, xl6_crypto_text_or_empty(text), textSize, digest) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    return xrtHexEncodeNew(digest, sizeof(digest), 1u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_chacha20_bytes(void* key, size_t keySize, void* nonce, size_t nonceSize, void* data, size_t size, unsigned int counter)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char* output; void* result;\n", out);
    xl6OutputPuts("    if ( key == 0 || keySize != 32u || nonce == 0 || nonceSize != 12u || (data == 0 && size != 0u) ) { return 0; }\n", out);
    xl6OutputPuts("    if ( size == 0u ) { return xl6_crypto_buffer_from_bytes(0, 0); }\n", out);
    xl6OutputPuts("    output = (unsigned char*)xrtMalloc(size); if ( output == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( !xrtChaCha20(key, nonce, counter, data, output, size) ) { xrtFree(output); return 0; }\n", out);
    xl6OutputPuts("    result = xl6_crypto_buffer_from_bytes(output, size); xrtFree(output); return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_chacha20_encrypt_hex(const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* text, size_t textSize, unsigned int counter)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char* out;\n", out);
    xl6OutputPuts("    void* hex;\n", out);
    xl6OutputPuts("    if ( keySize != 32u || nonceSize != 12u ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( textSize == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    out = (unsigned char*)xrtMalloc(textSize);\n", out);
    xl6OutputPuts("    if ( out == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xrtChaCha20(key, nonce, counter, xl6_crypto_text_or_empty(text), out, textSize) ) { xrtFree(out); return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    hex = xl6_crypto_hex_from_bytes(out, textSize);\n", out);
    xl6OutputPuts("    xrtFree(out);\n", out);
    xl6OutputPuts("    return hex;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_chacha20_decrypt_text(const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* cipherHex, size_t cipherHexSize, unsigned int counter)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t cipherSize;\n", out);
    xl6OutputPuts("    unsigned char* cipher;\n", out);
    xl6OutputPuts("    unsigned char* out;\n", out);
    xl6OutputPuts("    if ( keySize != 32u || nonceSize != 12u ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_hex_text(cipherHex, cipherHexSize) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    cipherSize = cipherHexSize / 2u;\n", out);
    xl6OutputPuts("    cipher = xl6_crypto_hex_decode(cipherHex, cipherHexSize);\n", out);
    xl6OutputPuts("    if ( cipher == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    out = (unsigned char*)xrtMalloc(cipherSize + 1u);\n", out);
    xl6OutputPuts("    if ( out == 0 ) { xrtFree(cipher); return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xrtChaCha20(key, nonce, counter, cipher, out, cipherSize) ) { xrtFree(cipher); xrtFree(out); return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    xrtFree(cipher);\n", out);
    xl6OutputPuts("    out[cipherSize] = '\\0';\n", out);
    xl6OutputPuts("    return out;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 使用当前 XRT AEAD 契约封装 cipher || tag 输出。 */\n", out);
    xl6OutputPuts("static bool xl6_crypto_aead_seal(int algorithm, const void* key, size_t keySize, const void* nonce, size_t nonceSize, const void* aad, size_t aadSize, const void* plain, size_t plainSize, void* output, size_t outputSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xaesgcm state;\n", out);
    xl6OutputPuts("    bool ok;\n", out);
    xl6OutputPuts("    if ( algorithm == 0 ) {\n", out);
    xl6OutputPuts("        return xrtChaCha20Poly1305Seal(key, nonce, aad, aadSize, plain, plainSize, output, outputSize);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( !xrtAesGcmInit(&state, key, keySize, 16u) ) { return false; }\n", out);
    xl6OutputPuts("    ok = xrtAesGcmSeal(&state, nonce, nonceSize, aad, aadSize, plain, plainSize, output, outputSize);\n", out);
    xl6OutputPuts("    xrtAesGcmClear(&state);\n", out);
    xl6OutputPuts("    return ok;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 使用当前 XRT AEAD 契约验证并打开 cipher || tag 输入。 */\n", out);
    xl6OutputPuts("static bool xl6_crypto_aead_open(int algorithm, const void* key, size_t keySize, const void* nonce, size_t nonceSize, const void* aad, size_t aadSize, const void* input, size_t inputSize, void* plain, size_t plainSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xaesgcm state;\n", out);
    xl6OutputPuts("    bool ok;\n", out);
    xl6OutputPuts("    if ( algorithm == 0 ) {\n", out);
    xl6OutputPuts("        return xrtChaCha20Poly1305Open(key, nonce, aad, aadSize, input, inputSize, plain, plainSize);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( !xrtAesGcmInit(&state, key, keySize, 16u) ) { return false; }\n", out);
    xl6OutputPuts("    ok = xrtAesGcmOpen(&state, nonce, nonceSize, aad, aadSize, input, inputSize, plain, plainSize);\n", out);
    xl6OutputPuts("    xrtAesGcmClear(&state);\n", out);
    xl6OutputPuts("    return ok;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_crypto_aead_encrypt_hex(int alg, const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* text, size_t textSize, const char* aad, size_t aadSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char* out;\n", out);
    xl6OutputPuts("    void* hex;\n", out);
    xl6OutputPuts("    bool ok = false;\n", out);
    xl6OutputPuts("    size_t outSize = textSize + 16u;\n", out);
    xl6OutputPuts("    if ( nonceSize != 12u ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( (alg == 1 && keySize != 16u) || (alg != 1 && keySize != 32u) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    out = (unsigned char*)xrtMalloc(outSize);\n", out);
    xl6OutputPuts("    if ( out == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    ok = xl6_crypto_aead_seal(alg, key, keySize, nonce, nonceSize, xl6_crypto_text_or_empty(aad), aadSize, xl6_crypto_text_or_empty(text), textSize, out, outSize);\n", out);
    xl6OutputPuts("    if ( !ok ) { xrtFree(out); return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    hex = xrtHexEncodeNew(out, outSize, 1u);\n", out);
    xl6OutputPuts("    xrtFree(out);\n", out);
    xl6OutputPuts("    return hex != 0 ? hex : xl6_crypto_empty_result();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_crypto_aead_decrypt_text(int alg, const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* cipherHex, size_t cipherHexSize, const char* aad, size_t aadSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t cipherSize;\n", out);
    xl6OutputPuts("    size_t plainSize;\n", out);
    xl6OutputPuts("    unsigned char* cipher;\n", out);
    xl6OutputPuts("    unsigned char* plain;\n", out);
    xl6OutputPuts("    bool ok = false;\n", out);
    xl6OutputPuts("    if ( nonceSize != 12u ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( (alg == 1 && keySize != 16u) || (alg != 1 && keySize != 32u) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_hex_text(cipherHex, cipherHexSize) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    cipherSize = cipherHexSize / 2u;\n", out);
    xl6OutputPuts("    if ( cipherSize < 16u ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    cipher = xl6_crypto_hex_decode(cipherHex, cipherHexSize);\n", out);
    xl6OutputPuts("    if ( cipher == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    plainSize = cipherSize - 16u;\n", out);
    xl6OutputPuts("    plain = (unsigned char*)xrtMalloc(plainSize + 1u);\n", out);
    xl6OutputPuts("    if ( plain == 0 ) { xrtFree(cipher); return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    ok = xl6_crypto_aead_open(alg, key, keySize, nonce, nonceSize, xl6_crypto_text_or_empty(aad), aadSize, cipher, cipherSize, plain, plainSize);\n", out);
    xl6OutputPuts("    xrtFree(cipher);\n", out);
    xl6OutputPuts("    if ( !ok ) { xrtFree(plain); return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    plain[plainSize] = '\\0';\n", out);
    xl6OutputPuts("    return plain;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_crypto_aead_bytes(int algorithm, int decrypt, const void* key, size_t keySize, const void* nonce, size_t nonceSize, const void* data, size_t size, const void* aad, size_t aadSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char* output; size_t outputSize; bool ok; void* result;\n", out);
    xl6OutputPuts("    if ( key == 0 || nonce == 0 || nonceSize == 0u || (data == 0 && size != 0u) || (aad == 0 && aadSize != 0u) ) { return 0; }\n", out);
    xl6OutputPuts("    if ( (algorithm == 0 && (keySize != 32u || nonceSize != 12u)) || (algorithm == 1 && keySize != 16u) || (algorithm == 2 && keySize != 32u) ) { return 0; }\n", out);
    xl6OutputPuts("    if ( decrypt && size < 16u ) { return 0; }\n", out);
    xl6OutputPuts("    if ( !decrypt && size > (size_t)-1 - 16u ) { return 0; }\n", out);
    xl6OutputPuts("    outputSize = decrypt ? size - 16u : size + 16u;\n", out);
    xl6OutputPuts("    output = (unsigned char*)xrtMalloc(outputSize > 0u ? outputSize : 1u); if ( output == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    ok = decrypt ? xl6_crypto_aead_open(algorithm, key, keySize, nonce, nonceSize, aad != 0 ? aad : \"\", aadSize, data != 0 ? data : \"\", size, output, outputSize) : xl6_crypto_aead_seal(algorithm, key, keySize, nonce, nonceSize, aad != 0 ? aad : \"\", aadSize, data != 0 ? data : \"\", size, output, outputSize);\n", out);
    xl6OutputPuts("    if ( !ok ) { xrtFree(output); return 0; }\n", out);
    xl6OutputPuts("    result = xl6_crypto_buffer_from_bytes(output, outputSize); xrtFree(output); return result;\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("void* xl6_xrt_chacha20_poly1305_encrypt_bytes(void* key, size_t keySize, void* nonce, size_t nonceSize, void* data, size_t size, void* aad, size_t aadSize) { return xl6_crypto_aead_bytes(0, 0, key, keySize, nonce, nonceSize, data, size, aad, aadSize); }\n", out);
    xl6OutputPuts("void* xl6_xrt_chacha20_poly1305_decrypt_bytes(void* key, size_t keySize, void* nonce, size_t nonceSize, void* data, size_t size, void* aad, size_t aadSize) { return xl6_crypto_aead_bytes(0, 1, key, keySize, nonce, nonceSize, data, size, aad, aadSize); }\n", out);
    xl6OutputPuts("void* xl6_xrt_aes128_gcm_encrypt_bytes(void* key, size_t keySize, void* nonce, size_t nonceSize, void* data, size_t size, void* aad, size_t aadSize) { return xl6_crypto_aead_bytes(1, 0, key, keySize, nonce, nonceSize, data, size, aad, aadSize); }\n", out);
    xl6OutputPuts("void* xl6_xrt_aes128_gcm_decrypt_bytes(void* key, size_t keySize, void* nonce, size_t nonceSize, void* data, size_t size, void* aad, size_t aadSize) { return xl6_crypto_aead_bytes(1, 1, key, keySize, nonce, nonceSize, data, size, aad, aadSize); }\n", out);
    xl6OutputPuts("void* xl6_xrt_aes256_gcm_encrypt_bytes(void* key, size_t keySize, void* nonce, size_t nonceSize, void* data, size_t size, void* aad, size_t aadSize) { return xl6_crypto_aead_bytes(2, 0, key, keySize, nonce, nonceSize, data, size, aad, aadSize); }\n", out);
    xl6OutputPuts("void* xl6_xrt_aes256_gcm_decrypt_bytes(void* key, size_t keySize, void* nonce, size_t nonceSize, void* data, size_t size, void* aad, size_t aadSize) { return xl6_crypto_aead_bytes(2, 1, key, keySize, nonce, nonceSize, data, size, aad, aadSize); }\n\n", out);

    xl6OutputPuts("void* xl6_xrt_chacha20_poly1305_encrypt_hex(const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* text, size_t textSize, const char* aad, size_t aadSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_crypto_aead_encrypt_hex(0, key, keySize, nonce, nonceSize, text, textSize, aad, aadSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_chacha20_poly1305_decrypt_text(const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* cipherHex, size_t cipherHexSize, const char* aad, size_t aadSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_crypto_aead_decrypt_text(0, key, keySize, nonce, nonceSize, cipherHex, cipherHexSize, aad, aadSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_aes128_gcm_encrypt_hex(const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* text, size_t textSize, const char* aad, size_t aadSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_crypto_aead_encrypt_hex(1, key, keySize, nonce, nonceSize, text, textSize, aad, aadSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_aes128_gcm_decrypt_text(const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* cipherHex, size_t cipherHexSize, const char* aad, size_t aadSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_crypto_aead_decrypt_text(1, key, keySize, nonce, nonceSize, cipherHex, cipherHexSize, aad, aadSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_aes256_gcm_encrypt_hex(const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* text, size_t textSize, const char* aad, size_t aadSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_crypto_aead_encrypt_hex(2, key, keySize, nonce, nonceSize, text, textSize, aad, aadSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_aes256_gcm_decrypt_text(const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* cipherHex, size_t cipherHexSize, const char* aad, size_t aadSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_crypto_aead_decrypt_text(2, key, keySize, nonce, nonceSize, cipherHex, cipherHexSize, aad, aadSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_crypto_hkdf_bytes(int algorithm, const void* ikm, size_t ikmSize, const void* salt, size_t saltSize, const void* info, size_t infoSize, size_t outSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char* output; void* result; bool ok;\n", out);
    xl6OutputPuts("    size_t maxSize = algorithm == 1 ? 8160u : 12240u;\n", out);
    xl6OutputPuts("    if ( outSize == 0u || outSize > maxSize || (ikm == 0 && ikmSize != 0u) || (salt == 0 && saltSize != 0u) || (info == 0 && infoSize != 0u) ) { return 0; }\n", out);
    xl6OutputPuts("    output = (unsigned char*)xrtMalloc(outSize); if ( output == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( algorithm == 1 ) { ok = xrtHkdfSha256(salt != 0 ? salt : \"\", saltSize, ikm != 0 ? ikm : \"\", ikmSize, info != 0 ? info : \"\", infoSize, output, outSize); }\n", out);
    xl6OutputPuts("    else { ok = xrtHkdfSha384(salt != 0 ? salt : \"\", saltSize, ikm != 0 ? ikm : \"\", ikmSize, info != 0 ? info : \"\", infoSize, output, outSize); }\n", out);
    xl6OutputPuts("    if ( !ok ) { xrtFree(output); return 0; }\n", out);
    xl6OutputPuts("    result = xl6_crypto_buffer_from_bytes(output, outSize); xrtFree(output); return result;\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("void* xl6_xrt_hkdf_sha256_bytes(void* ikm, size_t ikmSize, void* salt, size_t saltSize, void* info, size_t infoSize, size_t outSize) { return xl6_crypto_hkdf_bytes(1, ikm, ikmSize, salt, saltSize, info, infoSize, outSize); }\n", out);
    xl6OutputPuts("void* xl6_xrt_hkdf_sha384_bytes(void* ikm, size_t ikmSize, void* salt, size_t saltSize, void* info, size_t infoSize, size_t outSize) { return xl6_crypto_hkdf_bytes(2, ikm, ikmSize, salt, saltSize, info, infoSize, outSize); }\n\n", out);

    xl6OutputPuts("void* xl6_xrt_hkdf_sha256_hex(const char* ikm, size_t ikmSize, const char* salt, size_t saltSize, const char* info, size_t infoSize, size_t outSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char* okm;\n", out);
    xl6OutputPuts("    void* hex;\n", out);
    xl6OutputPuts("    if ( outSize == 0 || outSize > 8160u ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    okm = (unsigned char*)xrtMalloc(outSize);\n", out);
    xl6OutputPuts("    if ( okm == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xrtHkdfSha256(xl6_crypto_text_or_empty(salt), saltSize, xl6_crypto_text_or_empty(ikm), ikmSize, xl6_crypto_text_or_empty(info), infoSize, okm, outSize) ) { xrtFree(okm); return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    hex = xrtHexEncodeNew(okm, outSize, 1u);\n", out);
    xl6OutputPuts("    xrtFree(okm);\n", out);
    xl6OutputPuts("    return hex != 0 ? hex : xl6_crypto_empty_result();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_hkdf_sha384_hex(const char* ikm, size_t ikmSize, const char* salt, size_t saltSize, const char* info, size_t infoSize, size_t outSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char* okm;\n", out);
    xl6OutputPuts("    void* hex;\n", out);
    xl6OutputPuts("    if ( outSize == 0 || outSize > 12240u ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    okm = (unsigned char*)xrtMalloc(outSize);\n", out);
    xl6OutputPuts("    if ( okm == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xrtHkdfSha384(xl6_crypto_text_or_empty(salt), saltSize, xl6_crypto_text_or_empty(ikm), ikmSize, xl6_crypto_text_or_empty(info), infoSize, okm, outSize) ) { xrtFree(okm); return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    hex = xrtHexEncodeNew(okm, outSize, 1u);\n", out);
    xl6OutputPuts("    xrtFree(okm);\n", out);
    xl6OutputPuts("    return hex != 0 ? hex : xl6_crypto_empty_result();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xvalue* xl6_xrt_ed25519_keypair(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char seed[32];\n", out);
    xl6OutputPuts("    unsigned char pub[32];\n", out);
    xl6OutputPuts("    xvalue* table;\n", out);
    xl6OutputPuts("    if ( !xrtEd25519KeyPair(seed, pub) ) { return 0; }\n", out);
    xl6OutputPuts("    table = xrtValueObject();\n", out);
    xl6OutputPuts("    if ( table == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xl6_crypto_table_set_hex(table, \"seed\", seed, sizeof(seed));\n", out);
    xl6OutputPuts("    xl6_crypto_table_set_hex(table, \"publicKey\", pub, sizeof(pub));\n", out);
    xl6OutputPuts("    return table;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_ed25519_public_key_hex(const char* seedHex, size_t seedHexSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char seed[32];\n", out);
    xl6OutputPuts("    unsigned char pub[32];\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(seedHex, seedHexSize, sizeof(seed), seed) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xrtEd25519Public(seed, pub) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    return xl6_crypto_hex_from_bytes(pub, sizeof(pub));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_ed25519_sign_hex(const char* seedHex, size_t seedHexSize, const char* text, size_t textSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char seed[32];\n", out);
    xl6OutputPuts("    unsigned char sig[64];\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(seedHex, seedHexSize, sizeof(seed), seed) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xrtEd25519Sign(seed, xl6_crypto_text_or_empty(text), textSize, sig) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    return xl6_crypto_hex_from_bytes(sig, sizeof(sig));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_ed25519_verify(const char* publicKeyHex, size_t publicKeyHexSize, const char* text, size_t textSize, const char* signatureHex, size_t signatureHexSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char pub[32];\n", out);
    xl6OutputPuts("    unsigned char sig[64];\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(publicKeyHex, publicKeyHexSize, sizeof(pub), pub) ) { return false; }\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(signatureHex, signatureHexSize, sizeof(sig), sig) ) { return false; }\n", out);
    xl6OutputPuts("    return xrtEd25519Verify(pub, xl6_crypto_text_or_empty(text), textSize, sig);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xvalue* xl6_xrt_x25519_keypair(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char priv[32];\n", out);
    xl6OutputPuts("    unsigned char pub[32];\n", out);
    xl6OutputPuts("    xvalue* table;\n", out);
    xl6OutputPuts("    if ( !xrtX25519KeyPair(priv, pub) ) { return 0; }\n", out);
    xl6OutputPuts("    table = xrtValueObject();\n", out);
    xl6OutputPuts("    if ( table == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xl6_crypto_table_set_hex(table, \"privateKey\", priv, sizeof(priv));\n", out);
    xl6OutputPuts("    xl6_crypto_table_set_hex(table, \"publicKey\", pub, sizeof(pub));\n", out);
    xl6OutputPuts("    return table;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_x25519_shared_secret_hex(const char* privateKeyHex, size_t privateKeyHexSize, const char* publicKeyHex, size_t publicKeyHexSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char priv[32];\n", out);
    xl6OutputPuts("    unsigned char pub[32];\n", out);
    xl6OutputPuts("    unsigned char secret[32];\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(privateKeyHex, privateKeyHexSize, sizeof(priv), priv) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(publicKeyHex, publicKeyHexSize, sizeof(pub), pub) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xrtX25519Shared(priv, pub, secret) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    return xl6_crypto_hex_from_bytes(secret, sizeof(secret));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xvalue* xl6_xrt_x448_keypair(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char priv[56];\n", out);
    xl6OutputPuts("    unsigned char pub[56];\n", out);
    xl6OutputPuts("    xvalue* table;\n", out);
    xl6OutputPuts("    if ( !xrtX448KeyPair(priv, pub) ) { return 0; }\n", out);
    xl6OutputPuts("    table = xrtValueObject();\n", out);
    xl6OutputPuts("    if ( table == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xl6_crypto_table_set_hex(table, \"privateKey\", priv, sizeof(priv));\n", out);
    xl6OutputPuts("    xl6_crypto_table_set_hex(table, \"publicKey\", pub, sizeof(pub));\n", out);
    xl6OutputPuts("    return table;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_x448_shared_secret_hex(const char* privateKeyHex, size_t privateKeyHexSize, const char* publicKeyHex, size_t publicKeyHexSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char priv[56];\n", out);
    xl6OutputPuts("    unsigned char pub[56];\n", out);
    xl6OutputPuts("    unsigned char secret[56];\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(privateKeyHex, privateKeyHexSize, sizeof(priv), priv) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(publicKeyHex, publicKeyHexSize, sizeof(pub), pub) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xrtX448Shared(priv, pub, secret) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    return xl6_crypto_hex_from_bytes(secret, sizeof(secret));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xvalue* xl6_xrt_p256_keypair(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char priv[32];\n", out);
    xl6OutputPuts("    unsigned char pub[65];\n", out);
    xl6OutputPuts("    xvalue* table;\n", out);
    xl6OutputPuts("    if ( !xrtP256KeyPair(priv, pub) ) { return 0; }\n", out);
    xl6OutputPuts("    table = xrtValueObject();\n", out);
    xl6OutputPuts("    if ( table == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xl6_crypto_table_set_hex(table, \"privateKey\", priv, sizeof(priv));\n", out);
    xl6OutputPuts("    xl6_crypto_table_set_hex(table, \"publicKey\", pub, sizeof(pub));\n", out);
    xl6OutputPuts("    return table;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_p256_shared_secret_hex(const char* privateKeyHex, size_t privateKeyHexSize, const char* publicKeyHex, size_t publicKeyHexSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char priv[32];\n", out);
    xl6OutputPuts("    unsigned char pub[65];\n", out);
    xl6OutputPuts("    unsigned char secret[32];\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(privateKeyHex, privateKeyHexSize, sizeof(priv), priv) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(publicKeyHex, publicKeyHexSize, sizeof(pub), pub) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xrtP256Shared(priv, pub, secret) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    return xl6_crypto_hex_from_bytes(secret, sizeof(secret));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xvalue* xl6_xrt_p384_keypair(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char priv[48];\n", out);
    xl6OutputPuts("    unsigned char pub[97];\n", out);
    xl6OutputPuts("    xvalue* table;\n", out);
    xl6OutputPuts("    if ( !xrtP384KeyPair(priv, pub) ) { return 0; }\n", out);
    xl6OutputPuts("    table = xrtValueObject();\n", out);
    xl6OutputPuts("    if ( table == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xl6_crypto_table_set_hex(table, \"privateKey\", priv, sizeof(priv));\n", out);
    xl6OutputPuts("    xl6_crypto_table_set_hex(table, \"publicKey\", pub, sizeof(pub));\n", out);
    xl6OutputPuts("    return table;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_p384_shared_secret_hex(const char* privateKeyHex, size_t privateKeyHexSize, const char* publicKeyHex, size_t publicKeyHexSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char priv[48];\n", out);
    xl6OutputPuts("    unsigned char pub[97];\n", out);
    xl6OutputPuts("    unsigned char secret[48];\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(privateKeyHex, privateKeyHexSize, sizeof(priv), priv) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(publicKeyHex, publicKeyHexSize, sizeof(pub), pub) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xrtP384Shared(priv, pub, secret) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    return xl6_crypto_hex_from_bytes(secret, sizeof(secret));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_ecdsa_verify(const char* publicKeyHex, size_t publicKeyHexSize, const char* hashHex, size_t hashHexSize, const char* signatureHex, size_t signatureHexSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char* pub;\n", out);
    xl6OutputPuts("    unsigned char* hash;\n", out);
    xl6OutputPuts("    unsigned char* sig;\n", out);
    xl6OutputPuts("    size_t pubSize;\n", out);
    xl6OutputPuts("    size_t hashSize;\n", out);
    xl6OutputPuts("    size_t sigSize;\n", out);
    xl6OutputPuts("    bool ok;\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_hex_text(publicKeyHex, publicKeyHexSize) || !xl6_crypto_hex_text(hashHex, hashHexSize) || !xl6_crypto_hex_text(signatureHex, signatureHexSize) ) { return false; }\n", out);
    xl6OutputPuts("    pubSize = publicKeyHexSize / 2u;\n", out);
    xl6OutputPuts("    hashSize = hashHexSize / 2u;\n", out);
    xl6OutputPuts("    sigSize = signatureHexSize / 2u;\n", out);
    xl6OutputPuts("    if ( (pubSize != 65u && pubSize != 97u) || hashSize == 0u || sigSize == 0u ) { return false; }\n", out);
    xl6OutputPuts("    pub = xl6_crypto_hex_decode(publicKeyHex, publicKeyHexSize);\n", out);
    xl6OutputPuts("    hash = xl6_crypto_hex_decode(hashHex, hashHexSize);\n", out);
    xl6OutputPuts("    sig = xl6_crypto_hex_decode(signatureHex, signatureHexSize);\n", out);
    xl6OutputPuts("    if ( pub == 0 || hash == 0 || sig == 0 ) { xrtFree(pub); xrtFree(hash); xrtFree(sig); return false; }\n", out);
    xl6OutputPuts("    if ( pubSize == 65u ) {\n", out);
    xl6OutputPuts("        ok = sigSize == 64u ? xrtEcdsaP256Verify(hash, hashSize, sig, pub) : xrtEcdsaP256VerifyDer(hash, hashSize, sig, sigSize, pub);\n", out);
    xl6OutputPuts("    } else {\n", out);
    xl6OutputPuts("        ok = sigSize == 96u ? xrtEcdsaP384Verify(hash, hashSize, sig, pub) : xrtEcdsaP384VerifyDer(hash, hashSize, sig, sigSize, pub);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xrtFree(pub);\n", out);
    xl6OutputPuts("    xrtFree(hash);\n", out);
    xl6OutputPuts("    xrtFree(sig);\n", out);
    xl6OutputPuts("    return ok;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 根据摘要长度恢复标准摘要算法标识。 */\n", out);
    xl6OutputPuts("static xcryptohash xl6_crypto_hash_algorithm(size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( size == 20u ) { return XCRYPTO_HASH_SHA1; }\n", out);
    xl6OutputPuts("    if ( size == 28u ) { return XCRYPTO_HASH_SHA224; }\n", out);
    xl6OutputPuts("    if ( size == 32u ) { return XCRYPTO_HASH_SHA256; }\n", out);
    xl6OutputPuts("    if ( size == 48u ) { return XCRYPTO_HASH_SHA384; }\n", out);
    xl6OutputPuts("    if ( size == 64u ) { return XCRYPTO_HASH_SHA512; }\n", out);
    xl6OutputPuts("    return (xcryptohash)0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_crypto_rsa_verify(int alg, const char* hashHex, size_t hashHexSize, const char* sigHex, size_t sigHexSize, const char* modHex, size_t modHexSize, const char* expHex, size_t expHexSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char* hash;\n", out);
    xl6OutputPuts("    unsigned char* sig;\n", out);
    xl6OutputPuts("    unsigned char* mod;\n", out);
    xl6OutputPuts("    unsigned char* exp;\n", out);
    xl6OutputPuts("    xrsapublickey key;\n", out);
    xl6OutputPuts("    xcryptohash hashAlgorithm;\n", out);
    xl6OutputPuts("    bool ok;\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_hex_text(hashHex, hashHexSize) || !xl6_crypto_hex_text(sigHex, sigHexSize) || !xl6_crypto_hex_text(modHex, modHexSize) || !xl6_crypto_hex_text(expHex, expHexSize) ) { return false; }\n", out);
    xl6OutputPuts("    hashAlgorithm = xl6_crypto_hash_algorithm(hashHexSize / 2u);\n", out);
    xl6OutputPuts("    if ( hashAlgorithm == (xcryptohash)0 ) { return false; }\n", out);
    xl6OutputPuts("    hash = xl6_crypto_hex_decode(hashHex, hashHexSize);\n", out);
    xl6OutputPuts("    sig = xl6_crypto_hex_decode(sigHex, sigHexSize);\n", out);
    xl6OutputPuts("    mod = xl6_crypto_hex_decode(modHex, modHexSize);\n", out);
    xl6OutputPuts("    exp = xl6_crypto_hex_decode(expHex, expHexSize);\n", out);
    xl6OutputPuts("    if ( hash == 0 || sig == 0 || mod == 0 || exp == 0 ) { xrtFree(hash); xrtFree(sig); xrtFree(mod); xrtFree(exp); return false; }\n", out);
    xl6OutputPuts("    key.Modulus = mod;\n", out);
    xl6OutputPuts("    key.ModulusSize = modHexSize / 2u;\n", out);
    xl6OutputPuts("    key.Exponent = exp;\n", out);
    xl6OutputPuts("    key.ExponentSize = expHexSize / 2u;\n", out);
    xl6OutputPuts("    if ( alg == 0 ) { ok = xrtRsaPssVerify(&key, hashAlgorithm, hashAlgorithm, (size_t)-1, hash, sig, sigHexSize / 2u); }\n", out);
    xl6OutputPuts("    else { ok = xrtRsaPkcs1Verify(&key, hashAlgorithm, hash, sig, sigHexSize / 2u); }\n", out);
    xl6OutputPuts("    xrtFree(hash);\n", out);
    xl6OutputPuts("    xrtFree(sig);\n", out);
    xl6OutputPuts("    xrtFree(mod);\n", out);
    xl6OutputPuts("    xrtFree(exp);\n", out);
    xl6OutputPuts("    return ok;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_rsa_pss_verify(const char* hashHex, size_t hashHexSize, const char* sigHex, size_t sigHexSize, const char* modHex, size_t modHexSize, const char* expHex, size_t expHexSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_crypto_rsa_verify(0, hashHex, hashHexSize, sigHex, sigHexSize, modHex, modHexSize, expHex, expHexSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_rsa_pkcs1_verify(const char* hashHex, size_t hashHexSize, const char* sigHex, size_t sigHexSize, const char* modHex, size_t modHexSize, const char* expHex, size_t expHexSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_crypto_rsa_verify(1, hashHex, hashHexSize, sigHex, sigHexSize, modHex, modHexSize, expHex, expHexSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_random_bytes(size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char* data; void* result;\n", out);
    xl6OutputPuts("    if ( size > 4294967295u ) { return 0; }\n", out);
    xl6OutputPuts("    if ( size == 0u ) { return xl6_crypto_buffer_from_bytes(0, 0); }\n", out);
    xl6OutputPuts("    data = (unsigned char*)xrtMalloc(size); if ( data == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( !xrtSecureRandom(data, size) ) { xrtFree(data); return 0; }\n", out);
    xl6OutputPuts("    result = xl6_crypto_buffer_from_bytes(data, size); xrtFree(data); return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_random_hex(size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char* data;\n", out);
    xl6OutputPuts("    void* hex;\n", out);
    xl6OutputPuts("    if ( size == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    data = (unsigned char*)xrtMalloc(size);\n", out);
    xl6OutputPuts("    if ( data == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xrtSecureRandom(data, size) ) { xrtFree(data); return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    hex = xrtHexEncodeNew(data, size, 1u);\n", out);
    xl6OutputPuts("    xrtFree(data);\n", out);
    xl6OutputPuts("    return hex != 0 ? hex : xl6_crypto_empty_result();\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
