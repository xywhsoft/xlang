/* Small public-XRT-backed standard-library adapters.  Every family is enabled
 * only by an explicit, verified XLIR runtime bridge; this file never scans C
 * names or source declarations. */
static int xl6IrCgenEmitStdlibBridgeRuntime(
    XL6_Output* out, const XL6_IrCgenNeeds* needs)
{
    if ( out == NULL || needs == NULL ) return 0;
    if ( needs->textOutputBridgeOperations != 0 )
        xl6CgenEmitTextOutputRuntime(out, needs->textOutputBridgeOperations);
    if ( needs->dirBridgeOperations != 0 )
        xl6CgenEmitDirRuntime(out, needs->dirBridgeOperations);
    if ( needs->processRunBridgeOperations != 0 )
        xl6CgenEmitProcessRunRuntime(out, needs->processRunBridgeOperations);
    if ( needs->needsJsonVisitBridgeRuntime ||
         (needs->xsonBridgeOperations & XL6_CGEN_XSON_VISIT) )
        xl6CgenEmitSerializationCallbackRuntime(out, 1);
    if ( needs->xsonBridgeOperations != 0 )
        xl6CgenEmitXsonRuntime(out, needs->xsonBridgeOperations, 1);
    if ( needs->needsJsonVisitBridgeRuntime || needs->needsJsonWriterBridgeRuntime )
        xl6CgenEmitJsonStreamRuntime(out, needs->needsJsonVisitBridgeRuntime,
            needs->needsJsonWriterBridgeRuntime, 1);
    if ( needs->jsonValueBridgeOperations != 0 )
        xl6CgenEmitJsonValueRuntime(out, needs->jsonValueBridgeOperations,
            xl6IrCgenNeedsXrtDeclarations(needs));
    if ( needs->needsBase64TextCodecBridgeRuntime )
        xl6CgenEmitBase64TextCodecRuntime(out, xl6IrCgenNeedsXrtDeclarations(needs));
    if ( needs->needsHexBytesCodecBridgeRuntime )
        xl6CgenEmitHexBytesCodecRuntime(out, xl6IrCgenNeedsXrtDeclarations(needs));
    if ( needs->needsBase64BytesCodecBridgeRuntime )
        xl6CgenEmitBase64BytesCodecRuntime(out, xl6IrCgenNeedsXrtDeclarations(needs));
    if ( needs->needsLastErrorTextBridgeRuntime )
        xl6OutputPuts(
            "char* xl6_xrt_last_error_text(void)\n"
            "{ const xerror* error = xrtGetError(); const char* message = error != NULL ? xrtErrorMessage(error) : NULL; return xrtStrDup(message != NULL ? message : \"\"); }\n\n",
            out);
    if ( needs->needsStringCharLenBridgeRuntime )
        xl6OutputPuts(
            "extern size_t xrtUtf8Count(xstrview);\n"
            "size_t xl6_xrt_string_char_len(const char* text)\n"
            "{ return xrtUtf8Count(xrtStrView(text != NULL ? text : \"\")); }\n\n",
            out);
    if ( needs->needsTextCodecBridgeRuntime ||
         needs->needsCryptoTextHashBridgeRuntime )
        xl6OutputPuts(
            "extern size_t strlen(const char*);\n"
            "extern char* xrtStrDup(const char*);\n"
            "extern char* xrtHexEncodeNew(const void*, size_t, uint32_t);\n",
            out);
    if ( needs->needsTextCodecBridgeRuntime )
        xl6OutputPuts(
            "extern unsigned char* xrtHexDecodeNew(xstrview, size_t*, uint32_t);\n"
            "extern char* xrtPercentEncodeNew(const void*, size_t, xstrview, size_t*);\n"
            "extern unsigned char* xrtPercentDecodeNew(xstrview, size_t*);\n"
            "void* xl6_xrt_hex_encode_text(const char* text, size_t size)\n"
            "{ if (text == NULL) text = \"\"; if (size == 0u) size = strlen(text); return xrtHexEncodeNew(text, size, 1u); }\n"
            "void* xl6_xrt_hex_decode_text(const char* text, size_t size)\n"
            "{ size_t outputSize = 0u; if (text == NULL) text = \"\"; if (size == 0u) size = strlen(text); return xrtHexDecodeNew((xstrview){ text, size }, &outputSize, 0u); }\n"
            "void* xl6_xrt_percent_encode_text(const char* text, size_t size, void* outSize)\n"
            "{ size_t ignored = 0u; if (text == NULL) text = \"\"; return xrtPercentEncodeNew(text, size, (xstrview){ NULL, 0u }, outSize != NULL ? (size_t*)outSize : &ignored); }\n"
            "void* xl6_xrt_percent_decode_text(const char* text, size_t size, void* outSize)\n"
            "{ size_t ignored = 0u; if (text == NULL) text = \"\"; return xrtPercentDecodeNew((xstrview){ text, size }, outSize != NULL ? (size_t*)outSize : &ignored); }\n\n",
            out);
    if ( needs->needsCryptoTextHashBridgeRuntime )
        xl6OutputPuts(
            "extern bool xrtSha1(const void*, size_t, void*);\n"
            "extern bool xrtSha256(const void*, size_t, void*);\n"
            "extern bool xrtSha384(const void*, size_t, void*);\n"
            "extern bool xrtSha512(const void*, size_t, void*);\n"
            "static void* xlir_crypto_text_hash(const char* text, size_t size, unsigned int algorithm)\n"
            "{ unsigned char digest[64]; size_t digestSize; bool ok; if (text == NULL) { text = \"\"; size = 0u; } switch (algorithm) { case 1u: digestSize = 20u; ok = xrtSha1(text, size, digest); break; case 2u: digestSize = 32u; ok = xrtSha256(text, size, digest); break; case 3u: digestSize = 48u; ok = xrtSha384(text, size, digest); break; default: digestSize = 64u; ok = xrtSha512(text, size, digest); break; } return ok ? xrtHexEncodeNew(digest, digestSize, 1u) : xrtStrDup(\"\"); }\n"
            "void* xl6_xrt_sha1_hex(const char* text, size_t size) { return xlir_crypto_text_hash(text, size, 1u); }\n"
            "void* xl6_xrt_sha256_hex(const char* text, size_t size) { return xlir_crypto_text_hash(text, size, 2u); }\n"
            "void* xl6_xrt_sha384_hex(const char* text, size_t size) { return xlir_crypto_text_hash(text, size, 3u); }\n"
            "void* xl6_xrt_sha512_hex(const char* text, size_t size) { return xlir_crypto_text_hash(text, size, 4u); }\n\n",
            out);
    if ( needs->needsPathJoinBridgeRuntime )
        xl6OutputPuts(
            "extern char* xrtPathJoin(const char*, const char*);\n"
            "extern void xrtFree(void*);\n"
            "extern size_t xrtValueCount(const xvalue*);\n"
            "extern xvalue* xrtValueArrayGet(const xvalue*, size_t);\n"
            "extern bool xrtValueGetString(const xvalue*, xstrview*);\n"
            "void* xl6_xrt_path_join(const char* first, const char* second, xvalue* rest)\n"
            "{\n"
            "    size_t count = rest != NULL ? xrtValueCount(rest) : 0u; size_t i;\n"
            "    char* joined = xrtPathJoin(first != NULL ? first : \"\", second != NULL ? second : \"\");\n"
            "    if (joined == NULL) return NULL;\n"
            "    for (i = 0; i < count; ++i) {\n"
            "        xstrview part = {0}; char* next;\n"
            "        if (!xrtValueGetString(xrtValueArrayGet(rest, i), &part)) { xrtFree(joined); return NULL; }\n"
            "        next = xrtPathJoin(joined, part.Data != NULL ? part.Data : \"\");\n"
            "        xrtFree(joined); if (next == NULL) return NULL; joined = next;\n"
            "    }\n"
            "    return joined;\n"
            "}\n\n",
            out);
    return 1;
}
