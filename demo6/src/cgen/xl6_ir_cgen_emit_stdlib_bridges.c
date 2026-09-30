/* Small public-XRT-backed standard-library adapters.  Every family is enabled
 * only by an explicit, verified XLIR runtime bridge; this file never scans C
 * names or source declarations. */
static int xl6IrCgenEmitStdlibBridgeRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCgenNeeds* needs, XL6_DiagList* diags)
{
    if ( out == NULL || module == NULL || needs == NULL ) return 0;
    if ( needs->xidBridgeOperations != 0 &&
         !xl6IrCgenEmitXidRuntime(out, module, needs, diags) ) return 0;
    if ( needs->randomBridgeOperations != 0 )
        xl6IrCgenEmitRandomRuntime(out, needs->randomBridgeOperations);
    if ( needs->ioBridgeOperations != 0 )
        xl6IrCgenEmitIoRuntime(out, needs->ioBridgeOperations,
            xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_READER),
            xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_WRITER));
    if (needs->vfsBridgeOperations != 0)
        xl6IrCgenEmitVfsRuntime(out, needs->vfsBridgeOperations);
    if (needs->fsBridgeOperations0 != 0 || needs->fsBridgeOperations1 != 0 || needs->fsBridgeOperations2 != 0)
        xl6IrCgenEmitFsRuntime(out, needs);
    if ( needs->consoleBridgeOperations != 0 )
        xl6IrCgenEmitConsoleRuntime(out, needs->consoleBridgeOperations);
    if ( needs->textOutputBridgeOperations != 0 )
        xl6CgenEmitTextOutputRuntime(out, needs->textOutputBridgeOperations);
    if ( needs->dirBridgeOperations != 0 )
        xl6CgenEmitDirRuntime(out, needs->dirBridgeOperations);
    if ( needs->processRunBridgeOperations != 0 )
        xl6CgenEmitProcessRunRuntime(out, needs->processRunBridgeOperations);
    if ( needs->needsJsonVisitBridgeRuntime ||
         (needs->structuredOptionsBridgeOperations & XL6_CGEN_CONFIG_WRITE_TO_MASK) ||
         (needs->structuredOptionsBridgeOperations & XL6_CGEN_CONFIG_XLON_DECODE) ||
         (needs->xlonBridgeOperations & XL6_CGEN_XLON_VISIT) )
        xl6CgenEmitSerializationCallbackRuntime(out,
            needs->needsJsonVisitBridgeRuntime ||
            (needs->xlonBridgeOperations & XL6_CGEN_XLON_VISIT) ||
            (needs->structuredOptionsBridgeOperations & XL6_CGEN_CONFIG_WRITE_TO_MASK) ||
            (needs->structuredOptionsBridgeOperations &
                (XL6_CGEN_CONFIG_JSON_VISIT | XL6_CGEN_CONFIG_XLON_VISIT)));
    if ( needs->xlonBridgeOperations != 0 )
        xl6CgenEmitXlonRuntime(out, needs->xlonBridgeOperations);
    if ( needs->needsJsonVisitBridgeRuntime || needs->needsJsonWriterBridgeRuntime )
        xl6CgenEmitJsonStreamRuntime(out, needs->needsJsonVisitBridgeRuntime,
            needs->needsJsonWriterBridgeRuntime);
    if ( needs->jsonValueBridgeOperations != 0 )
        xl6CgenEmitJsonValueRuntime(out, needs->jsonValueBridgeOperations,
            xl6IrCgenNeedsXrtDeclarations(needs));
    if ( needs->structuredOptionsBridgeOperations != 0 )
        xl6CgenEmitStructuredOptionsRuntime(out, needs->structuredOptionsBridgeOperations);
    if ( needs->needsJsonlStringifyBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_jsonl_stringify(xvalue* records)\n"
            "{ return records != NULL ? xrtJsonlStringify(records, NULL) : NULL; }\n\n", out);
    if ( needs->needsXlonlStringifyBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_xlonl_stringify(xvalue* records)\n"
            "{ return records != NULL ? xrtXlonlStringify(records, NULL) : NULL; }\n\n", out);
    if ( needs->needsJsonlReadFileBridgeRuntime )
        xl6OutputPuts(
            "xvalue* xl6_xrt_jsonl_read_file(xlir_string path)\n"
            "{ const char* name = xlir_string_cstr(path); xvalue* value = name != NULL ? xrtJsonlParseFile(name) : NULL;\n"
            "  if (value == NULL) xrtClearError();\n  return value; }\n\n", out);
    if ( needs->needsJsonlWriteFileBridgeRuntime )
        xl6OutputPuts(
            "bool xl6_xrt_jsonl_write_file(xlir_string path, xvalue* records)\n"
            "{ const char* name = xlir_string_cstr(path); bool ok = name != NULL && records != NULL && xrtJsonlStringifyFile(name, records);\n"
            "  if (!ok) xrtClearError();\n  return ok; }\n\n", out);
    if ( needs->needsXlonlReadFileBridgeRuntime )
        xl6OutputPuts(
            "xvalue* xl6_xrt_xlonl_read_file(xlir_string path)\n"
            "{ const char* name = xlir_string_cstr(path); xvalue* value = name != NULL ? xrtXlonlParseFile(name) : NULL;\n"
            "  if (value == NULL) xrtClearError();\n  return value; }\n\n", out);
    if ( needs->needsXlonlWriteFileBridgeRuntime )
        xl6OutputPuts(
            "bool xl6_xrt_xlonl_write_file(xlir_string path, xvalue* records)\n"
            "{ const char* name = xlir_string_cstr(path); bool ok = name != NULL && records != NULL && xrtXlonlStringifyFile(name, records);\n"
            "  if (!ok) xrtClearError();\n  return ok; }\n\n", out);
    if ( needs->needsJsonlParseBridgeRuntime )
        xl6OutputPuts(
            "xvalue* xl6_xrt_jsonl_parse(xlir_string text)\n"
            "{ xvalue* value = text.Data != NULL ? xrtJsonlParse(text) : NULL;\n"
            "  if (value == NULL) xrtClearError();\n  return value; }\n\n", out);
    if ( needs->needsJsonlValidBridgeRuntime || needs->needsXlonlValidBridgeRuntime )
        xl6OutputPuts(
            "static bool xl6_serialization_syntax_error(const xerror* failure)\n"
            "{ const xerror* cause; if (failure == NULL || xrtErrorKind(failure) != XERR_PROTOCOL) return false;\n"
            "  for (cause = xrtErrorCause(failure); cause != NULL; cause = xrtErrorCause(cause))\n"
            "    if (xrtErrorKind(cause) != XERR_PROTOCOL && xrtErrorKind(cause) != XERR_VALUE) return false;\n"
            "  return true; }\n\n", out);
    if ( needs->needsJsonlValidBridgeRuntime )
        xl6OutputPuts(
            "bool xl6_xrt_jsonl_valid(xlir_string text)\n"
            "{ xerror* prior; xerror* failure; bool valid;\n"
            "  if (text.Data == NULL) return false;\n"
            "  prior = xrtTakeError(); valid = xrtJsonlValid(text); failure = xrtTakeError();\n"
            "  if (valid || xl6_serialization_syntax_error(failure)) {\n"
            "    xrtErrorFree(failure); xrtSetErrorTake(prior); return valid;\n"
            "  }\n"
            "  xrtErrorFree(prior); xrtSetErrorTake(failure); return false; }\n\n", out);
    if ( needs->needsXlonlParseBridgeRuntime )
        xl6OutputPuts(
            "xvalue* xl6_xrt_xlonl_parse(xlir_string text)\n"
            "{ xvalue* value = text.Data != NULL ? xrtXlonlParse(text) : NULL;\n"
            "  if (value == NULL) xrtClearError();\n  return value; }\n\n", out);
    if ( needs->needsXlonlValidBridgeRuntime )
        xl6OutputPuts(
            "bool xl6_xrt_xlonl_valid(xlir_string text)\n"
            "{ xerror* prior; xerror* failure; bool valid;\n"
            "  if (text.Data == NULL) return false;\n"
            "  prior = xrtTakeError(); valid = xrtXlonlValid(text); failure = xrtTakeError();\n"
            "  if (valid || xl6_serialization_syntax_error(failure)) {\n"
            "    xrtErrorFree(failure); xrtSetErrorTake(prior); return valid;\n"
            "  }\n"
            "  xrtErrorFree(prior); xrtSetErrorTake(failure); return false; }\n\n", out);
    if ( needs->needsBase64TextCodecBridgeRuntime )
        xl6CgenEmitBase64TextCodecRuntime(out, xl6IrCgenNeedsXrtDeclarations(needs));
    if ( needs->needsHexBytesCodecBridgeRuntime )
        xl6CgenEmitHexBytesCodecRuntime(out, xl6IrCgenNeedsXrtDeclarations(needs));
    if ( needs->needsBase64BytesCodecBridgeRuntime )
        xl6CgenEmitBase64BytesCodecRuntime(out, xl6IrCgenNeedsXrtDeclarations(needs));
    if ( needs->needsLastErrorTextBridgeRuntime )
        xl6OutputPuts(
            "extern size_t strlen(const char*);\n"
            "xlir_string xl6_xrt_last_error_text(void)\n"
            "{ const xerror* error = xrtGetError(); const char* message = error != NULL ? xrtErrorMessage(error) : NULL;\n"
            "  if (message == NULL) message = \"\";\n"
            "  return xlir_string_from_utf8(message, strlen(message)); }\n\n",
            out);
    if ( needs->needsStringCharLenBridgeRuntime )
        xl6OutputPuts(
            "extern size_t xrtUtf8Count(xstrview);\n"
            "size_t xl6_xrt_string_char_len(xlir_string text)\n"
            "{ return xrtUtf8Count(text); }\n\n",
            out);
    if ( needs->needsCrtNumberParseBridgeRuntime )
        xl6OutputPuts(
            "/* Exact-byte, locale-independent CRT-facing numeric parse ABI.\n"
            " * Parse failure is a value status; no transient XRT error escapes. */\n"
            "extern bool xrtIntParse(xstrview, uint32_t, uint32_t, int64_t*);\n"
            "extern bool xrtUIntParse(xstrview, uint32_t, uint32_t, uint64_t*);\n"
            "extern bool xrtNumParse(xstrview, uint32_t, double*);\n"
            "enum { XLIR_CRT_PARSE_SPECIAL = 1u << 3 }; /* XNUMBER_PARSE_SPECIAL */\n"
            "static int xlir_crt_parse_finish(bool ok, xerror* previous)\n"
            "{\n"
            "    int status = 0;\n"
            "    if (!ok) {\n"
            "        const xerror* error = xrtGetError();\n"
            "        xerrkind kind = error != NULL ? xrtErrorKind(error) : XERR_INTERNAL;\n"
            "        xerror* failed = xrtTakeError();\n"
            "        xrtErrorFree(failed);\n"
            "        status = kind == XERR_RANGE ? 2 :\n"
            "            (kind == XERR_PROTOCOL || kind == XERR_VALUE) ? 1 : 3;\n"
            "    }\n"
            "    if (previous != NULL) xrtSetErrorTake(previous);\n"
            "    return status;\n"
            "}\n"
            "int xl6_crt_parse_i64(xlir_string text, uint32_t base, int64_t* out)\n"
            "{\n"
            "    xerror* previous; int64_t value = 0; bool ok;\n"
            "    if (out == NULL) return 3;\n"
            "    previous = xrtTakeError();\n"
            "    ok = xrtIntParse(text, base, 0u, &value);\n"
            "    if (ok) *out = value;\n"
            "    return xlir_crt_parse_finish(ok, previous);\n"
            "}\n"
            "int xl6_crt_parse_u64(xlir_string text, uint32_t base, uint64_t* out)\n"
            "{\n"
            "    xerror* previous; uint64_t value = 0; bool ok;\n"
            "    if (out == NULL) return 3;\n"
            "    previous = xrtTakeError();\n"
            "    ok = xrtUIntParse(text, base, 0u, &value);\n"
            "    if (ok) *out = value;\n"
            "    return xlir_crt_parse_finish(ok, previous);\n"
            "}\n"
            "int xl6_crt_parse_f64(xlir_string text, double* out)\n"
            "{\n"
            "    xerror* previous; double value = 0.0; bool ok;\n"
            "    if (out == NULL) return 3;\n"
            "    previous = xrtTakeError();\n"
            "    ok = xrtNumParse(text, XLIR_CRT_PARSE_SPECIAL, &value);\n"
            "    if (ok) *out = value;\n"
            "    return xlir_crt_parse_finish(ok, previous);\n"
            "}\n\n",
            out);
    if ( needs->needsCrtEnvBridgeRuntime )
        xl6OutputPuts(
            "/* Environment is CString-based at the OS boundary.  Validate the\n"
            " * complete language value before passing its trailing-NUL data. */\n"
            "extern char* xrtEnvGet(const char*);\n"
            "extern bool xrtEnvLookup(const char*, char**);\n"
            "extern bool xrtEnvSet(const char*, const char*);\n"
            "extern bool xrtEnvRemove(const char*);\n"
            "void* xl6_crt_env_get(xlir_string name)\n"
            "{ const char* key = xlir_string_cstr(name); return key != NULL ? xrtEnvGet(key) : NULL; }\n"
            "bool xl6_crt_env_has(xlir_string name)\n"
            "{ const char* key = xlir_string_cstr(name); char* value = NULL; bool ok, found;\n"
            "  if (key == NULL) { return false; } ok = xrtEnvLookup(key, &value);\n"
            "  if (!ok) { return false; } found = value != NULL; xrtFree(value); return found; }\n"
            "bool xl6_crt_env_set(xlir_string name, xlir_string value)\n"
            "{ const char* key = xlir_string_cstr(name); const char* text =\n"
            "      key != NULL ? xlir_string_cstr(value) : NULL;\n"
            "  return key != NULL && text != NULL && xrtEnvSet(key, text); }\n"
            "bool xl6_crt_env_remove(xlir_string name)\n"
            "{ const char* key = xlir_string_cstr(name);\n"
            "  return key != NULL && xrtEnvRemove(key); }\n\n",
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
            "xlir_string xl6_xrt_hex_encode_text(xlir_string text, size_t size)\n"
            "{ char* output; if (size > text.Size) size = text.Size; if (size > SIZE_MAX / 2u) { xrtSetErrorKind(XERR_RANGE); return (xlir_string){0}; } output = xrtHexEncodeNew(text.Data, size, 1u); return xlir_string_take_known(output, output != NULL ? size * 2u : 0u); }\n"
            "xlir_string xl6_xrt_hex_decode_text(xlir_string text, size_t size)\n"
            "{ size_t outputSize = 0u; unsigned char* output; if (size > text.Size) size = text.Size; output = xrtHexDecodeNew((xstrview){ text.Data, size }, &outputSize, 0u); return xlir_string_take_known((char*)output, outputSize); }\n"
            "xlir_string xl6_xrt_percent_encode_text(xlir_string text, size_t size, void* outSize)\n"
            "{ size_t outputSize = 0u; char* output; if (size > text.Size) size = text.Size; output = xrtPercentEncodeNew(text.Data, size, (xstrview){ NULL, 0u }, &outputSize); if (outSize != NULL) *(size_t*)outSize = outputSize; return xlir_string_take_known(output, outputSize); }\n"
            "xlir_string xl6_xrt_percent_decode_text(xlir_string text, size_t size, void* outSize)\n"
            "{ size_t outputSize = 0u; unsigned char* output; if (size > text.Size) size = text.Size; output = xrtPercentDecodeNew((xstrview){ text.Data, size }, &outputSize); if (outSize != NULL) *(size_t*)outSize = outputSize; return xlir_string_take_known((char*)output, outputSize); }\n\n",
            out);
    if ( needs->needsCryptoTextHashBridgeRuntime )
        xl6OutputPuts(
            "extern bool xrtSha1(const void*, size_t, void*);\n"
            "extern bool xrtSha256(const void*, size_t, void*);\n"
            "extern bool xrtSha384(const void*, size_t, void*);\n"
            "extern bool xrtSha512(const void*, size_t, void*);\n"
            "static void* xlir_crypto_text_hash(xlir_string text, size_t size, unsigned int algorithm)\n"
            "{ unsigned char digest[64]; size_t digestSize; bool ok; if (text.Data == NULL) return NULL; if (size > text.Size) size = text.Size; switch (algorithm) { case 1u: digestSize = 20u; ok = xrtSha1(text.Data, size, digest); break; case 2u: digestSize = 32u; ok = xrtSha256(text.Data, size, digest); break; case 3u: digestSize = 48u; ok = xrtSha384(text.Data, size, digest); break; default: digestSize = 64u; ok = xrtSha512(text.Data, size, digest); break; } return ok ? xrtHexEncodeNew(digest, digestSize, 1u) : xrtStrDup(\"\"); }\n"
            "void* xl6_xrt_sha1_hex(xlir_string text, size_t size) { return xlir_crypto_text_hash(text, size, 1u); }\n"
            "void* xl6_xrt_sha256_hex(xlir_string text, size_t size) { return xlir_crypto_text_hash(text, size, 2u); }\n"
            "void* xl6_xrt_sha384_hex(xlir_string text, size_t size) { return xlir_crypto_text_hash(text, size, 3u); }\n"
            "void* xl6_xrt_sha512_hex(xlir_string text, size_t size) { return xlir_crypto_text_hash(text, size, 4u); }\n\n",
            out);
    if ( needs->needsHashStringBridgeRuntime ||
         needs->needsHashBytesBridgeRuntime ||
         needs->needsHashRawBridgeRuntime ||
         needs->needsSipHashStringBridgeRuntime ||
         needs->needsSipHashBytesBridgeRuntime ||
         needs->needsSipHashRawBridgeRuntime ||
         needs->needsSipHasher64BridgeRuntime ||
         needs->needsSipHasher64RawBridgeRuntime )
        xl6OutputPuts(
            "/* A negative limit means the complete value; public wrappers reject values below -1. */\n"
            "static size_t xlir_hash_limit(size_t actual, int64_t limit)\n"
            "{ return limit >= 0 && (uint64_t)limit < (uint64_t)actual ? (size_t)limit : actual; }\n\n",
            out);
    if ( needs->needsHashStringBridgeRuntime ||
         needs->needsHashBytesBridgeRuntime ||
         needs->needsHashRawBridgeRuntime )
        xl6OutputPuts(
            "extern uint32_t xrtHash32(const void*, size_t);\n"
            "extern uint32_t xrtHash32Seed(const void*, size_t, uint32_t);\n"
            "extern uint64_t xrtHash64(const void*, size_t);\n"
            "extern uint64_t xrtHash64Seed(const void*, size_t, uint64_t);\n\n",
            out);
    if ( needs->needsHashStringBridgeRuntime )
        xl6OutputPuts(
            "unsigned int xl6_xrt_hash32_string(xlir_string data, int64_t limit)\n"
            "{ return (unsigned int)xrtHash32(data.Data, xlir_hash_limit(data.Size, limit)); }\n"
            "unsigned int xl6_xrt_hash32_string_seed(xlir_string data, unsigned int seed, int64_t limit)\n"
            "{ return (unsigned int)xrtHash32Seed(data.Data, xlir_hash_limit(data.Size, limit), (uint32_t)seed); }\n"
            "unsigned long long xl6_xrt_hash64_string(xlir_string data, int64_t limit)\n"
            "{ return (unsigned long long)xrtHash64(data.Data, xlir_hash_limit(data.Size, limit)); }\n"
            "unsigned long long xl6_xrt_hash64_string_seed(xlir_string data, unsigned long long seed, int64_t limit)\n"
            "{ return (unsigned long long)xrtHash64Seed(data.Data, xlir_hash_limit(data.Size, limit), (uint64_t)seed); }\n\n",
            out);
    if ( needs->needsHashBytesBridgeRuntime ||
         needs->needsSipHashStringBridgeRuntime ||
         needs->needsSipHashBytesBridgeRuntime ||
         needs->needsSipHashRawBridgeRuntime ||
         needs->needsSipHasher64BridgeRuntime )
        xl6OutputPuts(
            "/* The statically typed bytes value owns this borrowed view for the entire call. */\n"
            "static xbytesview xlir_hash_bytes_view(const xvalue* value)\n"
            "{ xbytesview view = {0}; if (!xrtValueGetBytes(value, &view)) abort(); return view; }\n\n",
            out);
    if ( needs->needsHashBytesBridgeRuntime )
        xl6OutputPuts(
            "unsigned int xl6_xrt_hash32_bytes(xvalue* data, int64_t limit)\n"
            "{ xbytesview view = xlir_hash_bytes_view(data); return (unsigned int)xrtHash32(view.Data, xlir_hash_limit(view.Size, limit)); }\n"
            "unsigned int xl6_xrt_hash32_bytes_seed(xvalue* data, unsigned int seed, int64_t limit)\n"
            "{ xbytesview view = xlir_hash_bytes_view(data); return (unsigned int)xrtHash32Seed(view.Data, xlir_hash_limit(view.Size, limit), (uint32_t)seed); }\n"
            "unsigned long long xl6_xrt_hash64_bytes(xvalue* data, int64_t limit)\n"
            "{ xbytesview view = xlir_hash_bytes_view(data); return (unsigned long long)xrtHash64(view.Data, xlir_hash_limit(view.Size, limit)); }\n"
            "unsigned long long xl6_xrt_hash64_bytes_seed(xvalue* data, unsigned long long seed, int64_t limit)\n"
            "{ xbytesview view = xlir_hash_bytes_view(data); return (unsigned long long)xrtHash64Seed(view.Data, xlir_hash_limit(view.Size, limit), (uint64_t)seed); }\n\n",
            out);
    if ( needs->needsHashRawBridgeRuntime ||
         needs->needsSipHashRawBridgeRuntime ||
         needs->needsSipHasher64RawBridgeRuntime )
        xl6OutputPuts(
            "/* A nonempty raw view must originate from a valid Buffer allocation. */\n"
            "static const void* xlir_hash_raw_data(void* data, size_t actual)\n"
            "{ if (data == NULL && actual != 0u) abort(); return data; }\n\n",
            out);
    if ( needs->needsHashRawBridgeRuntime )
        xl6OutputPuts(
            "unsigned int xl6_xrt_hash32_raw(void* data, size_t actual, int64_t limit)\n"
            "{ return (unsigned int)xrtHash32(xlir_hash_raw_data(data, actual), xlir_hash_limit(actual, limit)); }\n"
            "unsigned int xl6_xrt_hash32_raw_seed(void* data, size_t actual, unsigned int seed, int64_t limit)\n"
            "{ return (unsigned int)xrtHash32Seed(xlir_hash_raw_data(data, actual), xlir_hash_limit(actual, limit), (uint32_t)seed); }\n"
            "unsigned long long xl6_xrt_hash64_raw(void* data, size_t actual, int64_t limit)\n"
            "{ return (unsigned long long)xrtHash64(xlir_hash_raw_data(data, actual), xlir_hash_limit(actual, limit)); }\n"
            "unsigned long long xl6_xrt_hash64_raw_seed(void* data, size_t actual, unsigned long long seed, int64_t limit)\n"
            "{ return (unsigned long long)xrtHash64Seed(xlir_hash_raw_data(data, actual), xlir_hash_limit(actual, limit), (uint64_t)seed); }\n\n",
            out);
    if ( needs->needsSipHashStringBridgeRuntime ||
         needs->needsSipHashBytesBridgeRuntime ||
         needs->needsSipHashRawBridgeRuntime ||
         needs->needsSipHasher64BridgeRuntime )
        xl6OutputPuts(
            "/* Decode the canonical 16-byte SipHash key without alignment or host-endian assumptions. */\n"
            "static int xlir_sip_key_read(const xvalue* value, xsipkey* out)\n"
            "{ xbytesview view = xlir_hash_bytes_view(value); uint64_t low = 0u, high = 0u; size_t i;\n"
            "  if (view.Size != 16u) { xrtSetErrorKind(XERR_VALUE); return 0; }\n"
            "  for (i = 0u; i < 8u; ++i) { low |= (uint64_t)view.Data[i] << (i * 8u); high |= (uint64_t)view.Data[i + 8u] << (i * 8u); }\n"
            "  *out = xrtSipKey(low, high); return 1; }\n\n",
            out);
    if ( needs->needsSipHashStringBridgeRuntime )
        xl6OutputPuts(
            "unsigned long long xl6_xrt_siphash64_string(xlir_string data, xvalue* keyValue, int64_t limit)\n"
            "{ xsipkey key; if (!xlir_sip_key_read(keyValue, &key)) abort();\n"
            "  return (unsigned long long)xrtSipHash(data.Data, xlir_hash_limit(data.Size, limit), key); }\n\n",
            out);
    if ( needs->needsSipHashBytesBridgeRuntime )
        xl6OutputPuts(
            "unsigned long long xl6_xrt_siphash64_bytes(xvalue* data, xvalue* keyValue, int64_t limit)\n"
            "{ xbytesview view = xlir_hash_bytes_view(data); xsipkey key; if (!xlir_sip_key_read(keyValue, &key)) abort();\n"
            "  return (unsigned long long)xrtSipHash(view.Data, xlir_hash_limit(view.Size, limit), key); }\n\n",
            out);
    if ( needs->needsSipHashRawBridgeRuntime )
        xl6OutputPuts(
            "unsigned long long xl6_xrt_siphash64_raw(void* data, size_t actual, xvalue* keyValue, int64_t limit)\n"
            "{ xsipkey key; if (!xlir_sip_key_read(keyValue, &key)) abort();\n"
            "  return (unsigned long long)xrtSipHash(xlir_hash_raw_data(data, actual), xlir_hash_limit(actual, limit), key); }\n\n",
            out);
    if ( needs->needsSipHasher64BridgeRuntime )
        xl6OutputPuts(
            "typedef struct xlir_siphasher64 { xsiphash state; xsipkey key; } xlir_siphasher64;\n"
            "void* xl6_xrt_siphasher64_create(xvalue* keyValue)\n"
            "{ xsipkey key; xlir_siphasher64* hasher; if (!xlir_sip_key_read(keyValue, &key)) return NULL;\n"
            "  hasher = (xlir_siphasher64*)xrtMalloc(sizeof(*hasher)); if (hasher == NULL) return NULL;\n"
            "  hasher->key = key; xrtSipHashInit(&hasher->state, key); return hasher; }\n"
            "void* xl6_xrt_siphasher64_adopt(void* handle) { if (handle == NULL) xrtSetErrorKind(XERR_STATE); return handle; }\n"
            "void xl6_xrt_siphasher64_destroy(void* self)\n"
            "{ if (self != NULL) { xrtSecureZero(self, sizeof(xlir_siphasher64)); xrtFree(self); } }\n"
            "bool xl6_xrt_siphasher64_update_string(void* self, const char* data, size_t actual, int64_t limit)\n"
            "{ if (self == NULL) { xrtSetErrorKind(XERR_STATE); return false; }\n"
            "  return xrtSipHashUpdate(&((xlir_siphasher64*)self)->state, data, xlir_hash_limit(actual, limit)); }\n"
            "bool xl6_xrt_siphasher64_update_bytes(void* self, xvalue* data, int64_t limit)\n"
            "{ xbytesview view = xlir_hash_bytes_view(data); if (self == NULL) { xrtSetErrorKind(XERR_STATE); return false; }\n"
            "  return xrtSipHashUpdate(&((xlir_siphasher64*)self)->state, view.Data, xlir_hash_limit(view.Size, limit)); }\n"
            "unsigned long long xl6_xrt_siphasher64_digest(void* self)\n"
            "{ if (self == NULL) { xrtSetErrorKind(XERR_STATE); return 0u; }\n"
            "  return (unsigned long long)xrtSipHashFinal(&((xlir_siphasher64*)self)->state); }\n"
            "void xl6_xrt_siphasher64_reset(void* self)\n"
            "{ if (self == NULL) { xrtSetErrorKind(XERR_STATE); return; }\n"
            "  { xlir_siphasher64* h = (xlir_siphasher64*)self; xrtSipHashInit(&h->state, h->key); } }\n"
            "void* xl6_xrt_siphasher64_clone(void* self)\n"
            "{ xlir_siphasher64* copy; if (self == NULL) { xrtSetErrorKind(XERR_STATE); return NULL; }\n"
            "  copy = (xlir_siphasher64*)xrtMalloc(sizeof(*copy));\n"
            "  if (copy != NULL) memcpy(copy, self, sizeof(*copy));\n"
            "  return copy; }\n"
            "unsigned long long xl6_xrt_siphasher64_input_bytes(void* self)\n"
            "{ if (self == NULL) { xrtSetErrorKind(XERR_STATE); return 0u; }\n"
            "  return (unsigned long long)((xlir_siphasher64*)self)->state.Total; }\n\n",
            out);
    if ( needs->needsSipHasher64RawBridgeRuntime )
        xl6OutputPuts(
            "bool xl6_xrt_siphasher64_update_raw(void* self, void* data, size_t actual, int64_t limit)\n"
            "{ if (self == NULL) { xrtSetErrorKind(XERR_STATE); return false; }\n"
            "  return xrtSipHashUpdate(&((xlir_siphasher64*)self)->state,\n"
            "    xlir_hash_raw_data(data, actual), xlir_hash_limit(actual, limit)); }\n\n",
            out);
    if ( needs->needsPathJoinBridgeRuntime )
        xl6OutputPuts(
            "extern char* xrtPathJoin(const char*, const char*);\n"
            "extern void xrtFree(void*);\n"
            "extern size_t xrtValueCount(const xvalue*);\n"
            "extern xvalue* xrtValueArrayGet(const xvalue*, size_t);\n"
            "extern bool xrtValueGetString(const xvalue*, xstrview*);\n"
            "void* xl6_xrt_path_join(xlir_string first, xlir_string second, xvalue* rest)\n"
            "{\n"
            "    size_t count = rest != NULL ? xrtValueCount(rest) : 0u; size_t i;\n"
            "    const char* a = xlir_string_cstr(first); const char* b = xlir_string_cstr(second);\n"
            "    char* joined = a != NULL && b != NULL ? xrtPathJoin(a, b) : NULL;\n"
            "    if (joined == NULL) return NULL;\n"
            "    for (i = 0; i < count; ++i) {\n"
            "        xstrview part = {0}; char* next; char* cpart;\n"
            "        if (!xrtValueGetString(xrtValueArrayGet(rest, i), &part) || part.Data == NULL) { xrtFree(joined); return NULL; }\n"
            "        if (memchr(part.Data, 0, part.Size) != NULL) { xrtFree(joined); xrtSetErrorKind(XERR_VALUE); return NULL; }\n"
            "        cpart = xrtStrDupN(part.Data, part.Size);\n"
            "        if (cpart == NULL) { xrtFree(joined); return NULL; }\n"
            "        next = xrtPathJoin(joined, cpart); xrtFree(cpart);\n"
            "        xrtFree(joined); if (next == NULL) return NULL; joined = next;\n"
            "    }\n"
            "    return joined;\n"
            "}\n\n",
            out);
    return 1;
}
