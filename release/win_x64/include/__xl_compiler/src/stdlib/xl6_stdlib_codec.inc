/* Safe value-facing codecs. Optional arguments are ordinary extern defaults,
 * resolved before XLIR; no generated wrappers or raw-pointer public entry. */
#define XL6_CODEC_DECL(Symbol, Name, Params, Result) \
    "extern \"" Symbol "\" @pure @nonblocking func " Name "(" Params ") as " Result "\n"
static const char xl6StdCodecHexEncodeBody[]=XL6_CODEC_DECL("xl6CodecHexTextV1","__xl_std_codec_hex_encode",
    "string data, int size = -1, int flags = 0","string");
static const char xl6StdCodecHexEncodeValueBody[]=XL6_CODEC_DECL("xl6CodecHexBytesV1","__xl_std_codec_hex_encode",
    "bytes data, int size = -1, int flags = 0","string")
    XL6_CODEC_DECL("xl6CodecHexBytesSpanV1","__xl_std_codec_hex_encode",
    "cbytes data, int size = -1, int flags = 0","string");
static const char xl6StdCodecHexDecodeBody[]=XL6_CODEC_DECL("xl6CodecHexDecodeV1","__xl_std_codec_hex_decode",
    "string text, int size = -1, int flags = 0","bytes");
static const char xl6StdCodecBase64EncodeBody[]=XL6_CODEC_DECL("xl6CodecBase64TextV1","__xl_std_codec_base64_encode",
    "string data, int size = -1, int flags = 0, string alphabet = \"\", int alphabetSize = -1","string");
static const char xl6StdCodecBase64EncodeValueBody[]=XL6_CODEC_DECL("xl6CodecBase64BytesV1","__xl_std_codec_base64_encode",
    "bytes data, int size = -1, int flags = 0, string alphabet = \"\", int alphabetSize = -1","string")
    XL6_CODEC_DECL("xl6CodecBase64BytesSpanV1","__xl_std_codec_base64_encode",
    "cbytes data, int size = -1, int flags = 0, string alphabet = \"\", int alphabetSize = -1","string");
static const char xl6StdCodecBase64DecodeBody[]=XL6_CODEC_DECL("xl6CodecBase64DecodeV1","__xl_std_codec_base64_decode",
    "string text, int size = -1, int flags = 0, string alphabet = \"\", int alphabetSize = -1","bytes");
static const char xl6StdCodecPercentEncodeBody[]=XL6_CODEC_DECL("xl6CodecPercentTextV1","__xl_std_codec_percent_encode",
    "string data, int size = -1, string safe = \"\", bool form = false, bool unreserved = true, int safeSize = -1","string");
static const char xl6StdCodecPercentEncodeValueBody[]=XL6_CODEC_DECL("xl6CodecPercentBytesV1","__xl_std_codec_percent_encode",
    "bytes data, int size = -1, string safe = \"\", bool form = false, bool unreserved = true, int safeSize = -1","string")
    XL6_CODEC_DECL("xl6CodecPercentBytesSpanV1","__xl_std_codec_percent_encode",
    "cbytes data, int size = -1, string safe = \"\", bool form = false, bool unreserved = true, int safeSize = -1","string");
static const char xl6StdCodecPercentDecodeBody[]=XL6_CODEC_DECL("xl6CodecPercentDecodeV1","__xl_std_codec_percent_decode",
    "string text, int size = -1, bool form = false","bytes");
#undef XL6_CODEC_DECL
