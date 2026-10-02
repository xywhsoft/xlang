static const char xl6StdEncodingEncodeBody[] =
    "extern \"xl6EncodingEncodeV1\" func __xl_std_encoding_encode(string text, int encoding = 1, bool bom = false, int policy = 0, int size = -1) as bytes\n";
static const char xl6StdEncodingDetectTextBody[] =
    "extern \"xl6EncodingGuessTextV1\" func __xl_std_encoding_detect(string text, int size = -1) as (int, int, int)\n";
static const char xl6StdEncodingDetectBytesBody[] =
    "extern \"xl6EncodingGuessBytesV1\" func __xl_std_encoding_detect(bytes data, int size = -1) as (int, int, int)\n";
static const char xl6StdEncodingDecodeBody[] =
    "extern \"xl6EncodingDecodeV1\" func __xl_std_encoding_decode(bytes data, int encoding = 1, int policy = 0, int bom = 1, int size = -1) as string\n";
static const char xl6StdEncodingTranscodeBody[] =
    "extern \"xl6EncodingTranscodeV1\" func __xl_std_encoding_transcode(bytes data, int source, int target, int policy = 0, int bom = 1, int size = -1) as bytes\n";
static const char xl6StdEncodingBomBody[] =
    "extern \"xl6EncodingBomV1\" func __xl_std_encoding_bom(bytes data, int size = -1) as int\n";
static const char xl6StdEncodingBomSizeBody[] =
    "extern \"xl6EncodingBomSizeV1\" func __xl_std_encoding_bomSize(bytes data, int size = -1) as int\n";
static const char xl6StdEncodingBomBytesBody[] =
    "extern \"xl6EncodingBomBytesV1\" func __xl_std_encoding_bomBytes(int encoding) as bytes\n";
static const char xl6StdEncodingUnitSizeBody[] =
    "extern \"xl6EncodingUnitSizeV1\" func __xl_std_encoding_unitSize(int encoding) as int\n";
static const char xl6StdEncodingNameBody[] =
    "extern \"xl6EncodingNameV1\" func __xl_std_encoding_name(int encoding) as string\n";
static const char xl6StdEncodingEncodeUtf8Body[] =
    "func __xl_std_encoding_encode_Utf8(string text, bool bom = false, int policy = 0, int size = -1) as bytes\n"
    "    return __xl_std_encoding_encode(text, 1, bom, policy, size)\n"
    "end\n";
static const char xl6StdEncodingDecodeUtf8Body[] =
    "func __xl_std_encoding_decode_Utf8(bytes data, int policy = 0, int bom = 1, int size = -1) as string\n"
    "    return __xl_std_encoding_decode(data, 1, policy, bom, size)\n"
    "end\n";
static const char xl6StdEncodingEncodeUtf16LEBody[] =
    "func __xl_std_encoding_encode_Utf16LE(string text, bool bom = false, int policy = 0, int size = -1) as bytes\n"
    "    return __xl_std_encoding_encode(text, 2, bom, policy, size)\n"
    "end\n";
static const char xl6StdEncodingDecodeUtf16LEBody[] =
    "func __xl_std_encoding_decode_Utf16LE(bytes data, int policy = 0, int bom = 1, int size = -1) as string\n"
    "    return __xl_std_encoding_decode(data, 2, policy, bom, size)\n"
    "end\n";
static const char xl6StdEncodingEncodeUtf16BEBody[] =
    "func __xl_std_encoding_encode_Utf16BE(string text, bool bom = false, int policy = 0, int size = -1) as bytes\n"
    "    return __xl_std_encoding_encode(text, 3, bom, policy, size)\n"
    "end\n";
static const char xl6StdEncodingDecodeUtf16BEBody[] =
    "func __xl_std_encoding_decode_Utf16BE(bytes data, int policy = 0, int bom = 1, int size = -1) as string\n"
    "    return __xl_std_encoding_decode(data, 3, policy, bom, size)\n"
    "end\n";
static const char xl6StdEncodingEncodeUtf32LEBody[] =
    "func __xl_std_encoding_encode_Utf32LE(string text, bool bom = false, int policy = 0, int size = -1) as bytes\n"
    "    return __xl_std_encoding_encode(text, 4, bom, policy, size)\n"
    "end\n";
static const char xl6StdEncodingDecodeUtf32LEBody[] =
    "func __xl_std_encoding_decode_Utf32LE(bytes data, int policy = 0, int bom = 1, int size = -1) as string\n"
    "    return __xl_std_encoding_decode(data, 4, policy, bom, size)\n"
    "end\n";
static const char xl6StdEncodingEncodeUtf32BEBody[] =
    "func __xl_std_encoding_encode_Utf32BE(string text, bool bom = false, int policy = 0, int size = -1) as bytes\n"
    "    return __xl_std_encoding_encode(text, 5, bom, policy, size)\n"
    "end\n";
static const char xl6StdEncodingDecodeUtf32BEBody[] =
    "func __xl_std_encoding_decode_Utf32BE(bytes data, int policy = 0, int bom = 1, int size = -1) as string\n"
    "    return __xl_std_encoding_decode(data, 5, policy, bom, size)\n"
    "end\n";
static const char xl6StdEncodingIsUtf8Body[] =
    "extern \"xl6TextIsUtf8V1\" func __xl_std_encoding_is_utf8(string text, int size = -1) as bool\n";
static const char xl6StdEncodingIsUtf8BytesBody[] =
    "extern \"xl6UnicodeInvalidBytesV1\" func __xl_encoding_invalid(bytes data, int size) as int\n"
    "func __xl_std_encoding_is_utf8(bytes data, int size = -1) as bool\n"
    "    return __xl_encoding_invalid(data, size) == -1\n"
    "end\n";
static const char xl6StdUnicodeDecodeBody[] =
    "func __xl_std_unicode_decode(bytes data, int policy = 0, int size = -1) as string\n"
    "    return __xl_std_encoding_decode(data, 1, policy, 0, size)\n"
    "end\n";
static const char xl6StdUnicodeRepairBody[] =
    "extern \"xl6EncodingRepairTextV1\" func __xl_std_unicode_repair(string text, int size = -1) as string\n";
