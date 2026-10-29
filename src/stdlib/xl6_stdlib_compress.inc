/* Plain language options are validated and snapshotted on each native call. */
const char* xl6StdlibCompressOptionsSource =
    "class DeflateOptions\n"
    "    int format = 2\n    int level = 6\n    int strategy = 0\n"
    "    int outputLimit = 67108864\n    int windowBits = 15\nend\n"
    "class InflateOptions\n"
    "    int format = 2\n    int outputLimit = 67108864\n"
    "    int gzipHeaderLimit = 65536\n    int windowBits = 15\nend\n";
const char* xl6StdlibDeflaterSource =
    "class Deflater\n    native point handle\n"
    "    extern \"xl6DeflaterCreateV1\" create(int format = 2, int level = 6, int strategy = 0, int outputLimit = 67108864, int windowBits = 15)\n"
    "    extern \"xl6DeflaterDestroyV1\" destroy()\n"
    "    extern \"xl6DeflaterWriteBytesV1\" func write(bytes data, Buffer output, int flush = 0, int size = -1) as bool\n"
    "    extern \"xl6DeflaterWriteBytesSpanV1\" func write(cbytes data, Buffer output, int flush = 0, int size = -1) as bool\n"
    "    extern \"xl6DeflaterWriteTextV1\" func write(string data, Buffer output, int flush = 0, int size = -1) as bool\n"
    "    extern \"xl6DeflaterWriteBufferV1\" func writeBuffer(Buffer data, Buffer output, int flush = 0, int size = -1) as bool\n"
    "    extern \"xl6DeflaterFinishV1\" func finish(Buffer output) as bool\n"
    "    extern \"xl6DeflaterDoneV1\" get done() as bool\n"
    "    extern \"xl6DeflaterFailedV1\" get failed() as bool\n"
    "    extern \"xl6DeflaterOutputSizeV1\" get outputSize() as uint64\n"
    "    extern \"xl6DeflaterCloseV1\" func close() as bool\n"
    "    extern \"xl6DeflaterClosedV1\" get closed() as bool\n"
    "    extern \"xl6DeflaterResetV1\" func resetNative(int format, int level, int strategy, int outputLimit, int windowBits) as bool\n"
    "    func reset(DeflateOptions? options = null) as bool\n"
    "        if options == null then return self.resetNative(2, 6, 0, 67108864, 15) end\n"
    "        return self.resetNative(options.format, options.level, options.strategy, options.outputLimit, options.windowBits)\n"
    "    end\nend\n";
const char* xl6StdlibInflaterSource =
    "class Inflater\n    native point handle\n"
    "    extern \"xl6InflaterCreateV1\" create(int format = 2, int outputLimit = 67108864, int gzipHeaderLimit = 65536, int windowBits = 15)\n"
    "    extern \"xl6InflaterDestroyV1\" destroy()\n"
    "    extern \"xl6InflaterWriteBytesV1\" func write(bytes data, Buffer output, bool final = false, int size = -1) as bool\n"
    "    extern \"xl6InflaterWriteBytesSpanV1\" func write(cbytes data, Buffer output, bool final = false, int size = -1) as bool\n"
    "    extern \"xl6InflaterWriteBufferV1\" func writeBuffer(Buffer data, Buffer output, bool final = false, int size = -1) as bool\n"
    "    extern \"xl6InflaterFinishV1\" func finish(Buffer output) as bool\n"
    "    extern \"xl6InflaterDoneV1\" get done() as bool\n"
    "    extern \"xl6InflaterFailedV1\" get failed() as bool\n"
    "    extern \"xl6InflaterOutputSizeV1\" get outputSize() as uint64\n"
    "    extern \"xl6InflaterCloseV1\" func close() as bool\n"
    "    extern \"xl6InflaterClosedV1\" get closed() as bool\n"
    "    extern \"xl6InflaterResetV1\" func resetNative(int format, int outputLimit, int gzipHeaderLimit, int windowBits) as bool\n"
    "    func reset(InflateOptions? options = null) as bool\n"
    "        if options == null then return self.resetNative(2, 67108864, 65536, 15) end\n"
    "        return self.resetNative(options.format, options.outputLimit, options.gzipHeaderLimit, options.windowBits)\n"
    "    end\nend\n";
#define XL6_DEFLATE_WHOLE(Symbol, Suffix, PublicSuffix, Type, Effect) \
    "extern \"" Symbol "\" " Effect "func __xl_compress_encode_" Suffix "(" Type " data, int size, int format, int level, int strategy, int outputLimit, int windowBits) as bytes\n" \
    "func __xl_std_compress_encode" PublicSuffix "(" Type " data, DeflateOptions? options = null, int size = -1) as bytes\n" \
    "    if options == null then return __xl_compress_encode_" Suffix "(data, size, 2, 6, 0, 67108864, 15) end\n" \
    "    return __xl_compress_encode_" Suffix "(data, size, options.format, options.level, options.strategy, options.outputLimit, options.windowBits)\nend\n"
static const char xl6StdCompressEncodeBody[] = XL6_DEFLATE_WHOLE("xl6DeflateTextV1", "", "", "string", "@pure @nonblocking ");
static const char xl6StdCompressEncodeBytesBody[] = XL6_DEFLATE_WHOLE("xl6DeflateBytesV1", "Bytes", "", "bytes", "@pure @nonblocking ")
    "extern \"xl6DeflateBytesSpanV1\" @pure @nonblocking func __xl_compress_encode_Bytes(cbytes data, int size, int format, int level, int strategy, int outputLimit, int windowBits) as bytes\n";
static const char xl6StdCompressEncodeBufferBody[] = XL6_DEFLATE_WHOLE("xl6DeflateBufferV1", "Buffer", "Buffer", "Buffer", "@nonblocking ");
#undef XL6_DEFLATE_WHOLE
#define XL6_INFLATE_WHOLE(Symbol, Suffix, Type, Effect) \
    "extern \"" Symbol "\" " Effect "func __xl_compress_decode_" Suffix "(" Type " data, int size, int format, int outputLimit, int gzipHeaderLimit, int windowBits) as bytes\n" \
    "func __xl_std_compress_decode" Suffix "(" Type " data, InflateOptions? options = null, int size = -1) as bytes\n" \
    "    if options == null then return __xl_compress_decode_" Suffix "(data, size, 2, 67108864, 65536, 15) end\n" \
    "    return __xl_compress_decode_" Suffix "(data, size, options.format, options.outputLimit, options.gzipHeaderLimit, options.windowBits)\nend\n"
static const char xl6StdCompressDecodeBody[] = XL6_INFLATE_WHOLE("xl6InflateBytesV1", "", "bytes", "@pure @nonblocking ")
    "extern \"xl6InflateBytesSpanV1\" @pure @nonblocking func __xl_compress_decode_(cbytes data, int size, int format, int outputLimit, int gzipHeaderLimit, int windowBits) as bytes\n";
static const char xl6StdCompressDecodeBufferBody[] = XL6_INFLATE_WHOLE("xl6InflateBufferV1", "Buffer", "Buffer", "@nonblocking ");
#undef XL6_INFLATE_WHOLE
static const char xl6StdCompressDeflaterBody[] =
    "func __xl_std_compress_deflater(DeflateOptions? options = null) as Deflater\n"
    "    if options == null then return Deflater() end\n"
    "    return Deflater(options.format, options.level, options.strategy, options.outputLimit, options.windowBits)\nend\n";
static const char xl6StdCompressInflaterBody[] =
    "func __xl_std_compress_inflater(InflateOptions? options = null) as Inflater\n"
    "    if options == null then return Inflater() end\n"
    "    return Inflater(options.format, options.outputLimit, options.gzipHeaderLimit, options.windowBits)\nend\n";
