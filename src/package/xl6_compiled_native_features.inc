/* Private NFM1 payload inside the XLP2 module literal. All lengths/counts are
 * little-endian u32. Bounds derive from the existing 4 MiB interface limit.
 * Validation and descriptor matching allocate nothing. Decode is transactional.
 */
#include <stdlib.h>
typedef struct XL6_NativeFeatureReader {
    const unsigned char* data;
    size_t size, offset;
    int failed;
} XL6_NativeFeatureReader;

static uint32_t xl6NativeFeatureU32(XL6_NativeFeatureReader* r)
{
    uint32_t n = 0;
    if (r->failed || r->offset > r->size || r->size - r->offset < 4) { r->failed = 1; return 0; }
    for (unsigned i = 0; i < 4; ++i) n |= (uint32_t)r->data[r->offset++] << (8u * i);
    return n;
}

static int xl6NativeFeatureIdentityValid(const char* text, size_t size, int name)
{
    if (!text || !size || size > 255) return 0;
    for (size_t i = 0; i < size; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (!name) { if (c < 33 || c > 126) return 0; }
        else if (!(c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                   (i && ((c >= '0' && c <= '9') || c == '.' || c == '-')))) return 0;
    }
    return 1;
}

static const char* xl6NativeFeatureString(XL6_NativeFeatureReader* r, size_t* size, int type)
{
    *size = xl6NativeFeatureU32(r);
    if (r->failed || *size > r->size - r->offset) { r->failed = 1; return NULL; }
    const char* data = (const char*)r->data + r->offset;
    r->offset += *size;
    if (!(type == 2 ? xl6PackageNativeFeatureKeyValid(data, *size) :
                     xl6NativeFeatureIdentityValid(data, *size, type == 0))) r->failed = 1;
    return r->failed ? NULL : data;
}

static int xl6NativeFeatureSameText(const char* text, size_t size, const char* other)
{
    return other && strlen(other) == size && !memcmp(text, other, size);
}

void xl6CompiledNativeFeaturesFree(XL6_CompiledNativeFeatures* data)
{
    if (!data) return;
    xrtFree(data->name); xrtFree(data->version); xrtFree(data->abiVersion);
    xl6PackageStringListFree(&data->required); xl6PackageStringListFree(&data->provided);
    xl6PackageStringListFree(&data->dependencies); memset(data, 0, sizeof(*data));
}

static int xl6NativeFeatureRead(const void* data, size_t size,
    XL6_CompiledNativeFeatures* out, const XL_CompiledPackage* descriptor)
{
    if (!data || size < 28 || size > XL_COMPILED_PACKAGE_MAX_INTERFACE || memcmp(data, "NFM1", 4)) return 0;
    XL6_NativeFeatureReader r = {(const unsigned char*)data, size, 4, 0};
    size_t n;
    for (unsigned i = 0; i < 3; ++i) {
        const char* text = xl6NativeFeatureString(&r, &n, i ? 1 : 0);
        if (r.failed) return 0;
        if (descriptor && !xl6NativeFeatureSameText(text, n, i == 0 ? descriptor->name :
            (i == 1 ? descriptor->packageVersion : descriptor->abiVersion))) return 0;
        if (out) {
            char** slot = i == 0 ? &out->name : (i == 1 ? &out->version : &out->abiVersion);
            *slot = xrtStrDupN(text, n); if (!*slot) return 0;
        }
    }
    for (unsigned list = 0; list < 3; ++list) {
        uint32_t count = xl6NativeFeatureU32(&r);
        /* Every entry requires at least its length plus one byte of text. */
        if (r.failed || count > (r.size - r.offset) / 5 ||
            (list == 2 && count > XL_COMPILED_PACKAGE_MAX_PACKAGES)) return 0;
        const char* previous = NULL; size_t previousSize = 0;
        for (uint32_t i = 0; i < count; ++i) {
            const char* text = xl6NativeFeatureString(&r, &n, list == 2 ? 0 : 2);
            if (r.failed) return 0;
            /* Wire lists are canonical, strictly increasing byte strings.
             * Linear validation needs no duplicate table or quadratic scan. */
            if (previous) {
                size_t common = previousSize < n ? previousSize : n;
                int order = memcmp(previous, text, common);
                if (order > 0 || (!order && previousSize >= n)) return 0;
            }
            previous = text; previousSize = n;
            if (descriptor && list == 2) {
                int found = 0;
                for (size_t d = 0; d < descriptor->dependencyCount; ++d) {
                    const XL_CompiledPackage* dep = descriptor->dependencies[d];
                    if (dep && xl6NativeFeatureSameText(text, n, dep->name)) { found = 1; break; }
                }
                if (!found) return 0;
            }
            if (out) {
                XL6_PackageStringList* target = list == 0 ? &out->required :
                    (list == 1 ? &out->provided : &out->dependencies);
                char* copy = xrtStrDupN(text, n);
                char** items = copy ? (char**)xrtRealloc(target->items, (target->count + 1) * sizeof(*items)) : NULL;
                if (!items) { xrtFree(copy); return 0; }
                target->items = items; target->items[target->count++] = copy;
            }
        }
    }
    return !r.failed && r.offset == r.size;
}

int xl6CompiledNativeFeaturesDecode(const void* data, size_t size, XL6_CompiledNativeFeatures* out)
{
    XL6_CompiledNativeFeatures result = {0};
    if (!out) return 0;
    if (!xl6NativeFeatureRead(data, size, &result, NULL)) { xl6CompiledNativeFeaturesFree(&result); return 0; }
    *out = result; return 1;
}

static void xl6NativeFeaturePutU32(unsigned char* data, size_t* offset, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) data[(*offset)++] = (unsigned char)(value >> (8u * i));
}

static int xl6NativeFeatureCompare(const void* a, const void* b)
{
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

int xl6CompiledNativeFeaturesEncode(const XL6_CompiledNativeFeatures* in, unsigned char** out, size_t* size)
{
    if (!in || !out || !size) return 0;
    const char* identity[] = {in->name, in->version, in->abiVersion};
    const XL6_PackageStringList* lists[] = {&in->required, &in->provided, &in->dependencies};
    size_t bytes = 4, maxCount = 0;
    for (unsigned i = 0; i < 3; ++i) {
        if (!identity[i] || !xl6NativeFeatureIdentityValid(identity[i], strlen(identity[i]), !i)) return 0;
        bytes += 4 + strlen(identity[i]);
    }
    for (unsigned l = 0; l < 3; ++l) {
        const XL6_PackageStringList* list = lists[l];
        if (list->count > UINT32_MAX || (list->count && !list->items) ||
            (l == 2 && list->count > XL_COMPILED_PACKAGE_MAX_PACKAGES)) return 0;
        if (list->count > maxCount) maxCount = list->count;
        bytes += 4;
        for (size_t i = 0; i < list->count; ++i) {
            const char* text = list->items[i]; if (!text) return 0;
            size_t n = strlen(text);
            if (!(l == 2 ? xl6NativeFeatureIdentityValid(text, n, 1) : xl6PackageNativeFeatureKeyValid(text, n)) ||
                bytes > XL_COMPILED_PACKAGE_MAX_INTERFACE - 4 || n > XL_COMPILED_PACKAGE_MAX_INTERFACE - bytes - 4) return 0;
            bytes += 4 + n;
        }
    }
    if (bytes > XL_COMPILED_PACKAGE_MAX_INTERFACE || maxCount > SIZE_MAX / sizeof(char*)) return 0;
    unsigned char* buffer = (unsigned char*)xrtMalloc(bytes); if (!buffer) return 0;
    const char** sorted = maxCount ? (const char**)xrtMalloc(maxCount * sizeof(*sorted)) : NULL;
    if (maxCount && !sorted) { xrtFree(buffer); return 0; }
    memcpy(buffer, "NFM1", 4); size_t pos = 4;
    for (unsigned i = 0; i < 3; ++i) {
        size_t n = strlen(identity[i]); xl6NativeFeaturePutU32(buffer, &pos, (uint32_t)n);
        memcpy(buffer + pos, identity[i], n); pos += n;
    }
    for (unsigned l = 0; l < 3; ++l) {
        if (lists[l]->count) {
            memcpy(sorted, lists[l]->items, lists[l]->count * sizeof(*sorted));
            qsort(sorted, lists[l]->count, sizeof(*sorted), xl6NativeFeatureCompare);
        }
        xl6NativeFeaturePutU32(buffer, &pos, (uint32_t)lists[l]->count);
        for (size_t i = 0; i < lists[l]->count; ++i) {
            if (i && !strcmp(sorted[i - 1], sorted[i])) { xrtFree(sorted); xrtFree(buffer); return 0; }
            size_t n = strlen(sorted[i]); xl6NativeFeaturePutU32(buffer, &pos, (uint32_t)n);
            memcpy(buffer + pos, sorted[i], n); pos += n;
        }
    }
    xrtFree(sorted);
    *out = buffer; *size = bytes; return 1;
}
