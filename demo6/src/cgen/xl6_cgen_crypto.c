static void xl6CgenEmitCryptoRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_CRYPTO_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_CRYPTO_RUNTIME\n", out);
    xl6OutputPuts("void xrtSHA1(const void* pData, size_t iLen, unsigned char* pOut);\n", out);
    xl6OutputPuts("void xrtSHA256(const void* pData, size_t iLen, unsigned char* pOut);\n", out);
    xl6OutputPuts("void xrtSHA384(const void* pData, size_t iLen, unsigned char* pOut);\n", out);
    xl6OutputPuts("void xrtSHA512(const void* pData, size_t iLen, unsigned char* pOut);\n", out);
    xl6OutputPuts("void* xrtHexEncode(void* pMem, size_t iSize);\n\n", out);

    xl6OutputPuts("static const char* xl6_crypto_text_or_empty(const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return text != 0 ? text : \"\";\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha1_hex(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[20];\n", out);
    xl6OutputPuts("    xrtSHA1((const void*)xl6_crypto_text_or_empty(text), size, digest);\n", out);
    xl6OutputPuts("    return xrtHexEncode((void*)digest, sizeof(digest));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha256_hex(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[32];\n", out);
    xl6OutputPuts("    xrtSHA256((const void*)xl6_crypto_text_or_empty(text), size, digest);\n", out);
    xl6OutputPuts("    return xrtHexEncode((void*)digest, sizeof(digest));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha384_hex(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[48];\n", out);
    xl6OutputPuts("    xrtSHA384((const void*)xl6_crypto_text_or_empty(text), size, digest);\n", out);
    xl6OutputPuts("    return xrtHexEncode((void*)digest, sizeof(digest));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha512_hex(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[64];\n", out);
    xl6OutputPuts("    xrtSHA512((const void*)xl6_crypto_text_or_empty(text), size, digest);\n", out);
    xl6OutputPuts("    return xrtHexEncode((void*)digest, sizeof(digest));\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
