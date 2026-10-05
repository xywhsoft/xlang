/* Emit only reached random adapters.  XRT remains the sole random-number
 * implementation; adapters own language values and translate error states. */
static void xl6IrCgenEmitRandomRuntime(XL6_Output* out, unsigned int operations)
{
    unsigned int explicit = operations & XL6_CGEN_RNG_EXPLICIT_MASK;
    unsigned int counted = operations & (XL6_CGEN_RNG_BYTES_MASK |
        XL6_CGEN_SECURE_HEX | XL6_CGEN_RNG_TEXT_MASK |
        XL6_CGEN_SECURE_STRING);
    unsigned int alphabet = operations & (XL6_CGEN_RNG_TEXT_MASK |
        XL6_CGEN_SECURE_STRING);
    if (operations & XL6_CGEN_RNG_ARRAY_MASK)
        xl6OutputPuts("#include <__xl_runtime/xl6_array_shuffle.h>\n", out);
    if (counted)
        xl6OutputPuts(
            "static bool xlir_random_count(int64_t requested, size_t* count)\n"
            "{ if (count == NULL || requested < 0 || (uint64_t)requested > SIZE_MAX)\n"
            "    { xrtSetErrorKind(XERR_RANGE); return false; }\n"
            "  *count = (size_t)requested; return true; }\n\n", out);
    if (alphabet)
        xl6OutputPuts(
            "static bool xlir_random_alphabet(const char* text, size_t actual, int64_t limit, xstrview* view)\n"
            "{ if (view == NULL || limit < -1) { xrtSetErrorKind(XERR_RANGE); return false; }\n"
            "  *view = (xstrview){ text, limit >= 0 && (uint64_t)limit < actual ?\n"
            "    (size_t)limit : actual }; return true; }\n\n", out);
    if (operations & XL6_CGEN_RNG_BYTES_MASK)
        xl6OutputPuts(
            "static xvalue* xlir_random_bytes_take(unsigned char* data, size_t count)\n"
            "{ xvalue* value = xrtValueBytesTake(&data, count);\n"
            "  if (data != NULL) { xrtSecureZero(data, count); xrtFree(data); }\n"
            "  return value; }\n\n", out);
    if (explicit)
        xl6OutputPuts(
            "typedef struct xlir_random_state { xrng rng; uint64_t owner; } xlir_random_state;\n\n", out);
    if (explicit & ~(XL6_CGEN_RNG_CREATE | XL6_CGEN_RNG_CREATE_DEFAULT |
            XL6_CGEN_RNG_DESTROY))
        xl6OutputPuts(
            "static xrng* xlir_random_state_get(void* handle)\n"
            "{ xlir_random_state* state = (xlir_random_state*)handle;\n"
            "  if (state == NULL || state->owner != xrtThreadCurrentId() || !xrtRngReady(&state->rng))\n"
            "    { xrtSetErrorKind(XERR_STATE); return NULL; }\n"
            "  return &state->rng; }\n\n", out);
    if (operations & (XL6_CGEN_RNG_CREATE | XL6_CGEN_RNG_CREATE_DEFAULT |
            XL6_CGEN_RNG_SEED))
        xl6OutputPuts(
            "static bool xlir_random_stream(uint64_t stream)\n"
            "{ if ((stream >> 63u) != 0) { xrtSetErrorKind(XERR_RANGE); return false; }\n"
            "  return true; }\n\n", out);
    if (operations & (XL6_CGEN_RNG_CREATE | XL6_CGEN_RNG_CREATE_DEFAULT))
        xl6OutputPuts(
            "static void* xlir_random_create(uint64_t seed, uint64_t stream)\n"
            "{ xlir_random_state* state; if (!xlir_random_stream(stream)) return NULL;\n"
            "  state = (xlir_random_state*)xrtMalloc(sizeof(*state));\n"
            "  if (state == NULL) return NULL;\n"
            "  state->owner = xrtThreadCurrentId();\n"
            "  xrtRngSeed(&state->rng, seed, stream); return state; }\n\n", out);
    if (operations & XL6_CGEN_RNG_CREATE)
        xl6OutputPuts(
            "void* xl6_xrt_rng_create(uint64_t seed, uint64_t stream)\n"
            "{ return xlir_random_create(seed, stream); }\n\n", out);
    if (operations & XL6_CGEN_RNG_CREATE_DEFAULT)
        xl6OutputPuts(
            "void* xl6_xrt_rng_create_default(uint64_t seed)\n"
            "{ return xlir_random_create(seed, 1u); }\n\n", out);
    if (operations & XL6_CGEN_RNG_DESTROY)
        xl6OutputPuts(
            "void xl6_xrt_rng_destroy(void* handle)\n"
            "{ if (handle != NULL) { xrtSecureZero(handle, sizeof(xlir_random_state)); xrtFree(handle); }\n"
            "}\n\n", out);
    if (operations & XL6_CGEN_RNG_SEED)
        xl6OutputPuts(
            "void xl6_xrt_rng_seed(void* handle, uint64_t seed, uint64_t stream)\n"
            "{ xrng* rng = xlir_random_state_get(handle);\n"
            "  if (rng != NULL && xlir_random_stream(stream)) xrtRngSeed(rng, seed, stream); }\n\n", out);
    if (operations & XL6_CGEN_RNG_U32)
        xl6OutputPuts(
            "unsigned int xl6_xrt_rng_u32(void* handle)\n"
            "{ xrng* rng = xlir_random_state_get(handle); return rng != NULL ? xrtRng32(rng) : 0; }\n\n", out);
    if (operations & XL6_CGEN_RNG_U64)
        xl6OutputPuts(
            "uint64_t xl6_xrt_rng_u64(void* handle)\n"
            "{ xrng* rng = xlir_random_state_get(handle); return rng != NULL ? xrtRng64(rng) : 0; }\n\n", out);
    if (operations & XL6_CGEN_RNG_BELOW)
        xl6OutputPuts(
            "uint64_t xl6_xrt_rng_below(void* handle, uint64_t bound)\n"
            "{ xrng* rng = xlir_random_state_get(handle); return rng != NULL ? xrtRngBelow64(rng, bound) : 0; }\n\n", out);
    if (operations & XL6_CGEN_RNG_RANGE)
        xl6OutputPuts(
            "int64_t xl6_xrt_rng_range(void* handle, int64_t min, int64_t max)\n"
            "{ xrng* rng = xlir_random_state_get(handle); return rng != NULL ? xrtRngRange(rng, min, max) : 0; }\n\n", out);
    if (operations & XL6_CGEN_RNG_RANGE_CLOSED)
        xl6OutputPuts(
            "int64_t xl6_xrt_rng_range_closed(void* handle, int64_t min, int64_t max)\n"
            "{ xrng* rng = xlir_random_state_get(handle); return rng != NULL ? xrtRngRangeClosed(rng, min, max) : 0; }\n\n", out);
    if (operations & XL6_CGEN_RNG_REAL)
        xl6OutputPuts(
            "double xl6_xrt_rng_real(void* handle)\n"
            "{ xrng* rng = xlir_random_state_get(handle); return rng != NULL ? xrtRngReal(rng) : 0.0; }\n\n", out);
    if (operations & XL6_CGEN_RNG_BYTES)
        xl6OutputPuts(
            "xvalue* xl6_xrt_rng_bytes(void* handle, int64_t requested)\n"
            "{ size_t count; unsigned char* data; xrng* rng = xlir_random_state_get(handle);\n"
            "  if (rng == NULL || !xlir_random_count(requested, &count)) return NULL;\n"
            "  data = (unsigned char*)xrtMalloc(count != 0 ? count : 1u);\n"
            "  if (data == NULL) return NULL;\n"
            "  if (!xrtRngBytes(rng, data, count)) { xrtSecureZero(data, count); xrtFree(data); return NULL; }\n"
            "  return xlir_random_bytes_take(data, count); }\n\n", out);
    if (operations & XL6_CGEN_RNG_STRING)
        xl6OutputPuts(
            "void* xl6_xrt_rng_string(void* handle, const char* alphabet, size_t actual, int64_t limit, int64_t requested)\n"
            "{ xstrview view; size_t count; char* text; xrng* rng = xlir_random_state_get(handle);\n"
            "  if (rng == NULL || !xlir_random_count(requested, &count) ||\n"
            "      !xlir_random_alphabet(alphabet, actual, limit, &view)) return NULL;\n"
            "  text = xrtRngStringFrom(rng, view, count); return text; }\n\n", out);
    if (operations & XL6_CGEN_RNG_FILL_BUFFER)
        xl6OutputPuts(
            "bool xl6_xrt_rng_fill_buffer(void* handle, void* target)\n"
            "{ xrng* rng = xlir_random_state_get(handle); xbuffer* buffer = (xbuffer*)target;\n"
            "  if (rng == NULL) return false;\n"
            "  if (buffer == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; }\n"
            "  return xrtRngBytes(rng, buffer->Data, buffer->Size); }\n\n", out);
    if (operations & XL6_CGEN_RNG_SHUFFLE_ARRAY)
        xl6OutputPuts(
            "bool xl6_xrt_rng_shuffle_array(void* handle, xvalue* values)\n"
            "{ xrng* rng = xlir_random_state_get(handle);\n"
            "  return rng != NULL && xl6ArrayRngShuffleV1(rng, values); }\n\n", out);
    if (operations & XL6_CGEN_RNG_SHUFFLE_BUFFER)
        xl6OutputPuts(
            "bool xl6_xrt_rng_shuffle_buffer(void* handle, void* target)\n"
            "{ xrng* rng = xlir_random_state_get(handle); xbuffer* buffer = (xbuffer*)target;\n"
            "  if (rng == NULL) return false;\n"
            "  if (buffer == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; }\n"
            "  return xrtRngShuffle(rng, buffer->Data, buffer->Size, 1u); }\n\n", out);
    if (operations & XL6_CGEN_RAND_SEED)
        xl6OutputPuts(
            "static bool xlir_random_default_stream(uint64_t stream)\n"
            "{ if ((stream >> 63u) != 0) { xrtSetErrorKind(XERR_RANGE); return false; }\n"
            "  return true; }\n\n", out);
    if (operations & XL6_CGEN_RAND_SEED)
        xl6OutputPuts(
            "void xl6_xrt_random_seed(uint64_t seed, uint64_t stream)\n"
            "{ if (xlir_random_default_stream(stream)) xrtRandSeed(seed, stream); }\n\n", out);
    if (operations & XL6_CGEN_RAND_U32)
        xl6OutputPuts(
            "unsigned int xl6_xrt_random_u32(void) { return xrtRand32(); }\n\n", out);
    if (operations & XL6_CGEN_RAND_U64)
        xl6OutputPuts(
            "uint64_t xl6_xrt_random_u64(void) { return xrtRand64(); }\n\n", out);
    if (operations & XL6_CGEN_RAND_BELOW)
        xl6OutputPuts(
            "uint64_t xl6_xrt_random_below(uint64_t bound) { return xrtRandBelow(bound); }\n\n", out);
    if (operations & XL6_CGEN_RAND_RANGE)
        xl6OutputPuts(
            "int64_t xl6_xrt_random_range(int64_t min, int64_t max)\n"
            "{ return xrtRandRange(min, max); }\n\n", out);
    if (operations & XL6_CGEN_RAND_RANGE_CLOSED)
        xl6OutputPuts(
            "int64_t xl6_xrt_random_range_closed(int64_t min, int64_t max)\n"
            "{ return xrtRandRangeClosed(min, max); }\n\n", out);
    if (operations & XL6_CGEN_RAND_REAL)
        xl6OutputPuts(
            "double xl6_xrt_random_real(void) { return xrtRandReal(); }\n\n", out);
    if (operations & XL6_CGEN_RAND_BYTES)
        xl6OutputPuts(
            "xvalue* xl6_xrt_random_bytes(int64_t requested)\n"
            "{ size_t count; unsigned char* data; if (!xlir_random_count(requested, &count)) return NULL;\n"
            "  data = (unsigned char*)xrtMalloc(count != 0 ? count : 1u);\n"
            "  if (data == NULL) return NULL;\n"
            "  if (!xrtRandBytes(data, count)) { xrtSecureZero(data, count); xrtFree(data); return NULL; }\n"
            "  return xlir_random_bytes_take(data, count); }\n\n", out);
    if (operations & XL6_CGEN_RAND_STRING)
        xl6OutputPuts(
            "void* xl6_xrt_random_string(const char* alphabet, size_t actual, int64_t limit, int64_t requested)\n"
            "{ xstrview view; size_t count; char* text;\n"
            "  if (!xlir_random_count(requested, &count) ||\n"
            "      !xlir_random_alphabet(alphabet, actual, limit, &view)) return NULL;\n"
            "  text = xrtRandStringFrom(view, count); return text; }\n\n", out);
    if (operations & XL6_CGEN_RAND_FILL_BUFFER)
        xl6OutputPuts(
            "bool xl6_xrt_random_fill_buffer(void* target)\n"
            "{ xbuffer* buffer = (xbuffer*)target;\n"
            "  if (buffer == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; }\n"
            "  return xrtRandBytes(buffer->Data, buffer->Size); }\n\n", out);
    if (operations & XL6_CGEN_RAND_SHUFFLE_ARRAY)
        xl6OutputPuts(
            "bool xl6_xrt_random_shuffle_array(xvalue* values)\n"
            "{ return xl6ArrayRandShuffleV1(values); }\n\n", out);
    if (operations & XL6_CGEN_RAND_SHUFFLE_BUFFER)
        xl6OutputPuts(
            "bool xl6_xrt_random_shuffle_buffer(void* target)\n"
            "{ xbuffer* buffer = (xbuffer*)target;\n"
            "  if (buffer == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; }\n"
            "  return xrtRandShuffle(buffer->Data, buffer->Size, 1u); }\n\n", out);
    if (operations & XL6_CGEN_SECURE_BYTES)
        xl6OutputPuts(
            "xvalue* xl6_xrt_secure_bytes(int64_t requested)\n"
            "{ size_t count; unsigned char* data; if (!xlir_random_count(requested, &count)) return NULL;\n"
            "  data = (unsigned char*)xrtMalloc(count != 0 ? count : 1u);\n"
            "  if (data == NULL) return NULL;\n"
            "  if (!xrtSecureRandom(data, count)) { xrtSecureZero(data, count); xrtFree(data); return NULL; }\n"
            "  return xlir_random_bytes_take(data, count); }\n\n", out);
    if (operations & XL6_CGEN_SECURE_FILL_BUFFER)
        xl6OutputPuts(
            "bool xl6_xrt_secure_fill_buffer(void* target)\n"
            "{ xbuffer* buffer = (xbuffer*)target;\n"
            "  if (buffer == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; }\n"
            "  return xrtSecureRandom(buffer->Data, buffer->Size); }\n\n", out);
    if (operations & XL6_CGEN_SECURE_HEX)
        xl6OutputPuts(
            "void* xl6_xrt_secure_hex(int64_t requested)\n"
            "{ size_t count; unsigned char* data; char* hex;\n"
            "  if (!xlir_random_count(requested, &count)) return NULL;\n"
            "  if (count > (SIZE_MAX - 1u) / 2u)\n"
            "    { xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
            "  data = (unsigned char*)xrtMalloc(count != 0 ? count : 1u);\n"
            "  if (data == NULL) return NULL;\n"
            "  if (!xrtSecureRandom(data, count))\n"
            "    { xrtSecureZero(data, count); xrtFree(data); return NULL; }\n"
            "  hex = xrtHexEncodeNew(data, count, 1u);\n"
            "  xrtSecureZero(data, count); xrtFree(data);\n"
            "  return hex; }\n\n", out);
    if (operations & XL6_CGEN_SECURE_STRING)
        xl6OutputPuts(
            "void* xl6_xrt_secure_string(const char* alphabet, size_t actual, int64_t limit, int64_t requested)\n"
            "{ xstrview view; size_t count; char* text;\n"
            "  if (!xlir_random_count(requested, &count) ||\n"
            "      !xlir_random_alphabet(alphabet, actual, limit, &view)) return NULL;\n"
            "  text = xrtSecureStringFrom(view, count);\n"
            "  return text; }\n\n", out);
}
