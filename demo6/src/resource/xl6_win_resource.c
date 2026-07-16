#include "src/resource/xl6_win_resource.h"

#include "lib/xrt/xrt.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)

#define XL6_RT_ICON 3u
#define XL6_RT_GROUP_ICON 14u
#define XL6_RT_VERSION 16u
#define XL6_RT_MANIFEST 24u

#define XL6_VS_FFI_SIGNATURE 0xFEEF04BDu
#define XL6_VS_FFI_STRUC_VERSION 0x00010000u
#define XL6_VOS_NT_WINDOWS32 0x00040004u
#define XL6_VFT_APP 0x00000001u

#if defined(__x86_64__) || defined(_M_X64)
#define XL6_COFF_MACHINE 0x8664u
#define XL6_COFF_RSRC_RELOC 3u
#elif defined(__aarch64__) || defined(_M_ARM64)
#define XL6_COFF_MACHINE 0xAA64u
#define XL6_COFF_RSRC_RELOC 3u
#elif defined(__i386__) || defined(_M_IX86)
#define XL6_COFF_MACHINE 0x014Cu
#define XL6_COFF_RSRC_RELOC 7u
#else
#define XL6_COFF_MACHINE 0x8664u
#define XL6_COFF_RSRC_RELOC 3u
#endif

typedef struct XL6_ResBuf {
    unsigned char* data;
    size_t size;
    size_t cap;
} XL6_ResBuf;

typedef struct XL6_WinResourceItem {
    unsigned int typeId;
    unsigned int nameId;
    unsigned int langId;
    unsigned int codepage;
    unsigned char* data;
    size_t size;
} XL6_WinResourceItem;

typedef struct XL6_WinResourceList {
    XL6_WinResourceItem* items;
    size_t count;
} XL6_WinResourceList;

typedef struct XL6_RelocList {
    unsigned int* offsets;
    size_t count;
} XL6_RelocList;

typedef struct XL6_DataEntryFixup {
    size_t dataEntryOffset;
    size_t itemIndex;
} XL6_DataEntryFixup;

typedef struct XL6_DataEntryFixupList {
    XL6_DataEntryFixup* items;
    size_t count;
} XL6_DataEntryFixupList;

static void xl6ResSetError(char* error, size_t errorSize, const char* fmt, ...)
{
    va_list ap;

    if ( error == NULL || errorSize == 0 ) {
        return;
    }
    va_start(ap, fmt);
    vsnprintf(error, errorSize, fmt != NULL ? fmt : "windows resource error", ap);
    va_end(ap);
    error[errorSize - 1] = '\0';
}

static int xl6ResBufReserve(XL6_ResBuf* buf, size_t extra)
{
    unsigned char* next;
    size_t need;
    size_t cap;

    if ( buf == NULL ) {
        return 0;
    }
    if ( extra > (size_t)-1 - buf->size ) {
        return 0;
    }
    need = buf->size + extra;
    if ( need <= buf->cap ) {
        return 1;
    }
    cap = buf->cap != 0 ? buf->cap : 4096u;
    while ( cap < need ) {
        if ( cap > (size_t)-1 / 2u ) {
            cap = need;
            break;
        }
        cap *= 2u;
    }
    next = (unsigned char*)xrtRealloc(buf->data, cap);
    if ( next == NULL ) {
        return 0;
    }
    buf->data = next;
    buf->cap = cap;
    return 1;
}

static int xl6ResBufWrite(XL6_ResBuf* buf, const void* data, size_t size)
{
    if ( size == 0 ) {
        return 1;
    }
    if ( data == NULL || !xl6ResBufReserve(buf, size) ) {
        return 0;
    }
    memcpy(buf->data + buf->size, data, size);
    buf->size += size;
    return 1;
}

static int xl6ResBufZero(XL6_ResBuf* buf, size_t size)
{
    if ( size == 0 ) {
        return 1;
    }
    if ( !xl6ResBufReserve(buf, size) ) {
        return 0;
    }
    memset(buf->data + buf->size, 0, size);
    buf->size += size;
    return 1;
}

static int xl6ResBufU8(XL6_ResBuf* buf, unsigned int value)
{
    unsigned char v = (unsigned char)(value & 0xFFu);
    return xl6ResBufWrite(buf, &v, 1u);
}

static int xl6ResBufU16(XL6_ResBuf* buf, unsigned int value)
{
    unsigned char v[2];

    v[0] = (unsigned char)(value & 0xFFu);
    v[1] = (unsigned char)((value >> 8) & 0xFFu);
    return xl6ResBufWrite(buf, v, sizeof(v));
}

static int xl6ResBufU32(XL6_ResBuf* buf, unsigned int value)
{
    unsigned char v[4];

    v[0] = (unsigned char)(value & 0xFFu);
    v[1] = (unsigned char)((value >> 8) & 0xFFu);
    v[2] = (unsigned char)((value >> 16) & 0xFFu);
    v[3] = (unsigned char)((value >> 24) & 0xFFu);
    return xl6ResBufWrite(buf, v, sizeof(v));
}

static int xl6ResBufAlign4(XL6_ResBuf* buf)
{
    size_t pad;

    if ( buf == NULL ) {
        return 0;
    }
    pad = (4u - (buf->size & 3u)) & 3u;
    return xl6ResBufZero(buf, pad);
}

static void xl6ResBufPatchU16(XL6_ResBuf* buf, size_t offset, unsigned int value)
{
    if ( buf == NULL || offset + 1u >= buf->size ) {
        return;
    }
    buf->data[offset] = (unsigned char)(value & 0xFFu);
    buf->data[offset + 1u] = (unsigned char)((value >> 8) & 0xFFu);
}

static void xl6ResBufPatchU32(XL6_ResBuf* buf, size_t offset, unsigned int value)
{
    if ( buf == NULL || offset + 3u >= buf->size ) {
        return;
    }
    buf->data[offset] = (unsigned char)(value & 0xFFu);
    buf->data[offset + 1u] = (unsigned char)((value >> 8) & 0xFFu);
    buf->data[offset + 2u] = (unsigned char)((value >> 16) & 0xFFu);
    buf->data[offset + 3u] = (unsigned char)((value >> 24) & 0xFFu);
}

static unsigned char* xl6ResBufTake(XL6_ResBuf* buf, size_t* outSize)
{
    unsigned char* data;

    if ( outSize != NULL ) {
        *outSize = buf != NULL ? buf->size : 0;
    }
    if ( buf == NULL ) {
        return NULL;
    }
    data = buf->data;
    buf->data = NULL;
    buf->size = 0;
    buf->cap = 0;
    return data;
}

static void xl6ResBufFree(XL6_ResBuf* buf)
{
    if ( buf == NULL ) {
        return;
    }
    xrtFree(buf->data);
    memset(buf, 0, sizeof(*buf));
}

static unsigned int xl6ResReadU16(const unsigned char* data, size_t size, size_t offset)
{
    if ( data == NULL || offset + 1u >= size ) {
        return 0;
    }
    return (unsigned int)data[offset] | ((unsigned int)data[offset + 1u] << 8);
}

static unsigned int xl6ResReadU32(const unsigned char* data, size_t size, size_t offset)
{
    if ( data == NULL || offset + 3u >= size ) {
        return 0;
    }
    return (unsigned int)data[offset] |
           ((unsigned int)data[offset + 1u] << 8) |
           ((unsigned int)data[offset + 2u] << 16) |
           ((unsigned int)data[offset + 3u] << 24);
}

static int xl6RelocAppend(XL6_RelocList* list, unsigned int offset)
{
    unsigned int* next;

    if ( list == NULL ) {
        return 0;
    }
    next = (unsigned int*)xrtRealloc(list->offsets, (list->count + 1u) * sizeof(unsigned int));
    if ( next == NULL ) {
        return 0;
    }
    list->offsets = next;
    list->offsets[list->count++] = offset;
    return 1;
}

static void xl6RelocFree(XL6_RelocList* list)
{
    if ( list == NULL ) {
        return;
    }
    xrtFree(list->offsets);
    memset(list, 0, sizeof(*list));
}

static int xl6FixupAppend(XL6_DataEntryFixupList* list, size_t dataEntryOffset, size_t itemIndex)
{
    XL6_DataEntryFixup* next;

    if ( list == NULL ) {
        return 0;
    }
    next = (XL6_DataEntryFixup*)xrtRealloc(list->items, (list->count + 1u) * sizeof(XL6_DataEntryFixup));
    if ( next == NULL ) {
        return 0;
    }
    list->items = next;
    list->items[list->count].dataEntryOffset = dataEntryOffset;
    list->items[list->count].itemIndex = itemIndex;
    list->count += 1u;
    return 1;
}

static void xl6FixupFree(XL6_DataEntryFixupList* list)
{
    if ( list == NULL ) {
        return;
    }
    xrtFree(list->items);
    memset(list, 0, sizeof(*list));
}

static int xl6ResourceListAdd(
    XL6_WinResourceList* list,
    unsigned int typeId,
    unsigned int nameId,
    unsigned int langId,
    unsigned int codepage,
    unsigned char* data,
    size_t size)
{
    XL6_WinResourceItem* next;

    if ( list == NULL || data == NULL || size == 0 ) {
        xrtFree(data);
        return 0;
    }
    next = (XL6_WinResourceItem*)xrtRealloc(list->items, (list->count + 1u) * sizeof(XL6_WinResourceItem));
    if ( next == NULL ) {
        xrtFree(data);
        return 0;
    }
    list->items = next;
    list->items[list->count].typeId = typeId;
    list->items[list->count].nameId = nameId;
    list->items[list->count].langId = langId;
    list->items[list->count].codepage = codepage;
    list->items[list->count].data = data;
    list->items[list->count].size = size;
    list->count += 1u;
    return 1;
}

static void xl6ResourceListFree(XL6_WinResourceList* list)
{
    size_t i;

    if ( list == NULL ) {
        return;
    }
    for ( i = 0; i < list->count; ++i ) {
        xrtFree(list->items[i].data);
    }
    xrtFree(list->items);
    memset(list, 0, sizeof(*list));
}

static int xl6ResourceCompare(const void* left, const void* right)
{
    const XL6_WinResourceItem* a = (const XL6_WinResourceItem*)left;
    const XL6_WinResourceItem* b = (const XL6_WinResourceItem*)right;

    if ( a->typeId != b->typeId ) {
        return a->typeId < b->typeId ? -1 : 1;
    }
    if ( a->nameId != b->nameId ) {
        return a->nameId < b->nameId ? -1 : 1;
    }
    if ( a->langId != b->langId ) {
        return a->langId < b->langId ? -1 : 1;
    }
    return 0;
}

static int xl6WriteUtf16String(XL6_ResBuf* buf, const char* text, size_t* outChars)
{
    u16str wide;
    size_t count;
    size_t i;

    if ( outChars != NULL ) {
        *outChars = 0;
    }
    if ( text == NULL ) {
        text = "";
    }
    wide = xrtUTF8to16((u8str)text, 0, &count);
    if ( wide == NULL ) {
        return 0;
    }
    for ( i = 0; i < count; ++i ) {
        if ( !xl6ResBufU16(buf, ((unsigned short*)wide)[i]) ) {
            xrtFree(wide);
            return 0;
        }
    }
    if ( !xl6ResBufU16(buf, 0) ) {
        xrtFree(wide);
        return 0;
    }
    xrtFree(wide);
    if ( outChars != NULL ) {
        *outChars = count;
    }
    return 1;
}

static size_t xl6BeginVersionBlock(XL6_ResBuf* buf, const char* key, unsigned int valueLength, unsigned int type)
{
    size_t start;

    start = buf->size;
    if ( !xl6ResBufU16(buf, 0) ||
         !xl6ResBufU16(buf, valueLength) ||
         !xl6ResBufU16(buf, type) ||
         !xl6WriteUtf16String(buf, key, NULL) ||
         !xl6ResBufAlign4(buf) ) {
        return (size_t)-1;
    }
    return start;
}

static int xl6EndVersionBlock(XL6_ResBuf* buf, size_t start)
{
    size_t length;

    if ( buf == NULL || start == (size_t)-1 || start > buf->size ) {
        return 0;
    }
    length = buf->size - start;
    if ( length > 0xFFFFu ) {
        return 0;
    }
    xl6ResBufPatchU16(buf, start, (unsigned int)length);
    return 1;
}

static int xl6WriteVersionStringBlock(XL6_ResBuf* buf, const char* key, const char* value)
{
    u16str wide;
    size_t valueChars;
    size_t start;

    if ( key == NULL || value == NULL || value[0] == '\0' ) {
        return 1;
    }
    wide = xrtUTF8to16((u8str)value, 0, &valueChars);
    if ( wide == NULL ) {
        return 0;
    }
    xrtFree(wide);
    start = xl6BeginVersionBlock(buf, key, (unsigned int)valueChars + 1u, 1u);
    if ( start == (size_t)-1 ||
         !xl6WriteUtf16String(buf, value, NULL) ||
         !xl6ResBufAlign4(buf) ||
         !xl6EndVersionBlock(buf, start) ) {
        return 0;
    }
    return 1;
}

static int xl6HasField(xvalue object, const char* name)
{
    return object != NULL && name != NULL &&
           xvoIsTable(object) &&
           xvoTableExists(object, (str)name, (uint32)strlen(name));
}

static xvalue xl6Field(xvalue object, const char* name)
{
    return xvoTableGetValue(object, (str)name, (uint32)strlen(name));
}

static const char* xl6TextField2(xvalue object, const char* a, const char* b)
{
    xvalue value;

    if ( xl6HasField(object, a) ) {
        value = xl6Field(object, a);
        if ( xvoIsText(value) ) {
            return xvoGetText(value);
        }
    }
    if ( b != NULL && xl6HasField(object, b) ) {
        value = xl6Field(object, b);
        if ( xvoIsText(value) ) {
            return xvoGetText(value);
        }
    }
    return NULL;
}

static unsigned int xl6UIntField(xvalue object, const char* name, unsigned int fallback)
{
    xvalue value;

    if ( !xl6HasField(object, name) ) {
        return fallback;
    }
    value = xl6Field(object, name);
    if ( xvoIsInt(value) ) {
        int64 v = xvoGetInt(value);
        return v < 0 ? fallback : (unsigned int)v;
    }
    if ( xvoIsText(value) ) {
        return (unsigned int)xrtStrToI64((str)xvoGetText(value));
    }
    return fallback;
}

static int xl6ParseVersionQuad(const char* text, unsigned int outParts[4])
{
    const char* p;
    char* end;
    int i;

    if ( outParts == NULL ) {
        return 0;
    }
    for ( i = 0; i < 4; ++i ) {
        outParts[i] = 0;
    }
    if ( text == NULL || text[0] == '\0' ) {
        return 1;
    }
    p = text;
    for ( i = 0; i < 4; ++i ) {
        unsigned long value;

        value = strtoul(p, &end, 10);
        if ( end == p || value > 65535u ) {
            return 0;
        }
        outParts[i] = (unsigned int)value;
        if ( *end == '\0' ) {
            return 1;
        }
        if ( *end != '.' ) {
            return 0;
        }
        p = end + 1;
    }
    return *p == '\0';
}

static const char* xl6VersionText(xvalue version, const char* camel, const char* official)
{
    const char* text;

    text = xl6TextField2(version, camel, official);
    return text != NULL ? text : "";
}

static int xl6AddVersionResource(
    XL6_WinResourceList* resources,
    xvalue version,
    unsigned int language,
    unsigned int codepage,
    char* error,
    size_t errorSize)
{
    static const struct {
        const char* camel;
        const char* official;
    } stringFields[] = {
        { "comments", "Comments" },
        { "companyName", "CompanyName" },
        { "fileDescription", "FileDescription" },
        { "fileVersion", "FileVersion" },
        { "internalName", "InternalName" },
        { "legalCopyright", "LegalCopyright" },
        { "originalFilename", "OriginalFilename" },
        { "productName", "ProductName" },
        { "productVersion", "ProductVersion" }
    };
    const char* fileVersion;
    const char* productVersion;
    unsigned int fv[4];
    unsigned int pv[4];
    char tableKey[16];
    XL6_ResBuf buf;
    size_t root;
    size_t stringFileInfo;
    size_t stringTable;
    size_t varFileInfo;
    size_t var;
    size_t dataSize;
    unsigned char* data;
    size_t i;

    if ( version == NULL || !xvoIsTable(version) ) {
        return 1;
    }
    memset(&buf, 0, sizeof(buf));
    fileVersion = xl6VersionText(version, "fileVersion", "FileVersion");
    productVersion = xl6VersionText(version, "productVersion", "ProductVersion");
    if ( fileVersion[0] == '\0' ) {
        fileVersion = productVersion[0] != '\0' ? productVersion : "0.0.0.0";
    }
    if ( productVersion[0] == '\0' ) {
        productVersion = fileVersion;
    }
    if ( !xl6ParseVersionQuad(fileVersion, fv) || !xl6ParseVersionQuad(productVersion, pv) ) {
        xl6ResSetError(error, errorSize, "invalid version string in resource config");
        return 0;
    }
    root = xl6BeginVersionBlock(&buf, "VS_VERSION_INFO", 52u, 0u);
    if ( root == (size_t)-1 ||
         !xl6ResBufU32(&buf, XL6_VS_FFI_SIGNATURE) ||
         !xl6ResBufU32(&buf, XL6_VS_FFI_STRUC_VERSION) ||
         !xl6ResBufU32(&buf, (fv[0] << 16) | fv[1]) ||
         !xl6ResBufU32(&buf, (fv[2] << 16) | fv[3]) ||
         !xl6ResBufU32(&buf, (pv[0] << 16) | pv[1]) ||
         !xl6ResBufU32(&buf, (pv[2] << 16) | pv[3]) ||
         !xl6ResBufU32(&buf, 0x3Fu) ||
         !xl6ResBufU32(&buf, 0u) ||
         !xl6ResBufU32(&buf, XL6_VOS_NT_WINDOWS32) ||
         !xl6ResBufU32(&buf, XL6_VFT_APP) ||
         !xl6ResBufU32(&buf, 0u) ||
         !xl6ResBufU32(&buf, 0u) ||
         !xl6ResBufU32(&buf, 0u) ||
         !xl6ResBufAlign4(&buf) ) {
        xl6ResBufFree(&buf);
        xl6ResSetError(error, errorSize, "cannot build version fixed info");
        return 0;
    }

    stringFileInfo = xl6BeginVersionBlock(&buf, "StringFileInfo", 0u, 1u);
    snprintf(tableKey, sizeof(tableKey), "%04x%04x", (unsigned int)(language & 0xFFFFu), (unsigned int)(codepage & 0xFFFFu));
    stringTable = xl6BeginVersionBlock(&buf, tableKey, 0u, 1u);
    for ( i = 0; i < sizeof(stringFields) / sizeof(stringFields[0]); ++i ) {
        const char* value = xl6VersionText(version, stringFields[i].camel, stringFields[i].official);
        if ( value[0] == '\0' && strcmp(stringFields[i].official, "FileVersion") == 0 ) {
            value = fileVersion;
        } else if ( value[0] == '\0' && strcmp(stringFields[i].official, "ProductVersion") == 0 ) {
            value = productVersion;
        }
        if ( !xl6WriteVersionStringBlock(&buf, stringFields[i].official, value) ) {
            xl6ResBufFree(&buf);
            xl6ResSetError(error, errorSize, "cannot build version string table");
            return 0;
        }
    }
    if ( !xl6EndVersionBlock(&buf, stringTable) ||
         !xl6EndVersionBlock(&buf, stringFileInfo) ) {
        xl6ResBufFree(&buf);
        xl6ResSetError(error, errorSize, "version string table is too large");
        return 0;
    }

    varFileInfo = xl6BeginVersionBlock(&buf, "VarFileInfo", 0u, 1u);
    var = xl6BeginVersionBlock(&buf, "Translation", 4u, 0u);
    if ( var == (size_t)-1 ||
         !xl6ResBufU16(&buf, language) ||
         !xl6ResBufU16(&buf, codepage) ||
         !xl6ResBufAlign4(&buf) ||
         !xl6EndVersionBlock(&buf, var) ||
         !xl6EndVersionBlock(&buf, varFileInfo) ||
         !xl6EndVersionBlock(&buf, root) ) {
        xl6ResBufFree(&buf);
        xl6ResSetError(error, errorSize, "cannot build version translation table");
        return 0;
    }
    data = xl6ResBufTake(&buf, &dataSize);
    if ( !xl6ResourceListAdd(resources, XL6_RT_VERSION, 1u, language, codepage, data, dataSize) ) {
        xl6ResSetError(error, errorSize, "cannot add version resource");
        return 0;
    }
    return 1;
}

static char* xl6ResolveRelativePath(const char* baseDir, const char* path)
{
    if ( path == NULL || path[0] == '\0' ) {
        return NULL;
    }
    if ( xrtPathIsAbs((str)path, 0) ) {
        return (char*)xrtCopyStr((str)path, 0);
    }
    return (char*)xrtPathJoin(2, (str)(baseDir != NULL && baseDir[0] != '\0' ? baseDir : "."), (str)path);
}

static int xl6LoadBinaryFile(
    const char* baseDir,
    const char* path,
    unsigned char** outData,
    size_t* outSize,
    char* error,
    size_t errorSize)
{
    char* resolved;
    unsigned char* data;
    size_t size;

    if ( outData == NULL || outSize == NULL ) {
        return 0;
    }
    *outData = NULL;
    *outSize = 0;
    resolved = xl6ResolveRelativePath(baseDir, path);
    if ( resolved == NULL ) {
        xl6ResSetError(error, errorSize, "resource file path is empty");
        return 0;
    }
    size = 0;
    data = (unsigned char*)xrtFileReadAll((str)resolved, XRT_CP_BINARY, &size);
    if ( data == NULL && size != 0 ) {
        xl6ResSetError(error, errorSize, "cannot read resource file: %s", resolved);
        xrtFree(resolved);
        return 0;
    }
    if ( data == NULL ) {
        data = (unsigned char*)xrtMalloc(1u);
        if ( data == NULL ) {
            xl6ResSetError(error, errorSize, "cannot allocate empty resource file");
            xrtFree(resolved);
            return 0;
        }
    }
    *outData = data;
    *outSize = size;
    xrtFree(resolved);
    return 1;
}

static int xl6DecodeBase64(
    const char* text,
    unsigned char** outData,
    size_t* outSize,
    char* error,
    size_t errorSize)
{
    xbuffer buf;
    unsigned char* data;

    if ( outData == NULL || outSize == NULL ) {
        return 0;
    }
    *outData = NULL;
    *outSize = 0;
    if ( text == NULL || text[0] == '\0' ) {
        xl6ResSetError(error, errorSize, "base64 resource data is empty");
        return 0;
    }
    buf = xrtBufferFromBase64((str)text, 0, NULL);
    if ( buf == NULL ) {
        xl6ResSetError(error, errorSize, "invalid base64 resource data");
        return 0;
    }
    data = (unsigned char*)xrtMalloc(buf->Length != 0 ? buf->Length : 1u);
    if ( data == NULL ) {
        xrtBufferDestroy(buf);
        xl6ResSetError(error, errorSize, "cannot allocate base64 resource data");
        return 0;
    }
    if ( buf->Length != 0 ) {
        memcpy(data, buf->Buffer, buf->Length);
    }
    *outData = data;
    *outSize = buf->Length;
    xrtBufferDestroy(buf);
    return 1;
}

static int xl6ReadBinarySpec(
    xvalue value,
    const char* baseDir,
    int allowText,
    unsigned char** outData,
    size_t* outSize,
    char* error,
    size_t errorSize)
{
    const char* text;

    if ( xvoIsText(value) ) {
        return xl6LoadBinaryFile(baseDir, xvoGetText(value), outData, outSize, error, errorSize);
    }
    if ( !xvoIsTable(value) ) {
        xl6ResSetError(error, errorSize, "resource field must be a string or object");
        return 0;
    }
    text = xl6TextField2(value, "path", NULL);
    if ( text != NULL ) {
        return xl6LoadBinaryFile(baseDir, text, outData, outSize, error, errorSize);
    }
    text = xl6TextField2(value, "base64", NULL);
    if ( text != NULL ) {
        return xl6DecodeBase64(text, outData, outSize, error, errorSize);
    }
    text = allowText ? xl6TextField2(value, "text", NULL) : NULL;
    if ( text != NULL ) {
        size_t size = strlen(text);
        unsigned char* data = (unsigned char*)xrtMalloc(size != 0 ? size : 1u);
        if ( data == NULL ) {
            xl6ResSetError(error, errorSize, "cannot allocate text resource data");
            return 0;
        }
        if ( size != 0 ) {
            memcpy(data, text, size);
        }
        *outData = data;
        *outSize = size;
        return 1;
    }
    xl6ResSetError(error, errorSize, "resource object requires path or base64");
    return 0;
}

static int xl6AddIconResources(
    XL6_WinResourceList* resources,
    xvalue icon,
    const char* baseDir,
    unsigned int language,
    unsigned int codepage,
    char* error,
    size_t errorSize)
{
    unsigned char* ico;
    size_t icoSize;
    unsigned int count;
    XL6_ResBuf group;
    unsigned int i;
    size_t groupSize;
    unsigned char* groupData;

    if ( icon == NULL || xvoIsNull(icon) ) {
        return 1;
    }
    ico = NULL;
    icoSize = 0;
    if ( !xl6ReadBinarySpec(icon, baseDir, 0, &ico, &icoSize, error, errorSize) ) {
        return 0;
    }
    if ( icoSize < 6u ||
         xl6ResReadU16(ico, icoSize, 0) != 0u ||
         xl6ResReadU16(ico, icoSize, 2) != 1u ) {
        xrtFree(ico);
        xl6ResSetError(error, errorSize, "icon resource must be a valid .ico file");
        return 0;
    }
    count = xl6ResReadU16(ico, icoSize, 4);
    if ( count == 0u || icoSize < 6u + (size_t)count * 16u ) {
        xrtFree(ico);
        xl6ResSetError(error, errorSize, "icon resource has invalid image table");
        return 0;
    }
    memset(&group, 0, sizeof(group));
    if ( !xl6ResBufU16(&group, 0u) ||
         !xl6ResBufU16(&group, 1u) ||
         !xl6ResBufU16(&group, count) ) {
        xrtFree(ico);
        xl6ResBufFree(&group);
        xl6ResSetError(error, errorSize, "cannot build group icon header");
        return 0;
    }
    for ( i = 0; i < count; ++i ) {
        size_t entry = 6u + (size_t)i * 16u;
        unsigned int bytesInRes = xl6ResReadU32(ico, icoSize, entry + 8u);
        unsigned int imageOffset = xl6ResReadU32(ico, icoSize, entry + 12u);
        unsigned char* imageData;

        if ( bytesInRes == 0u || imageOffset > icoSize || bytesInRes > icoSize - imageOffset ) {
            xrtFree(ico);
            xl6ResBufFree(&group);
            xl6ResSetError(error, errorSize, "icon resource image range is invalid");
            return 0;
        }
        imageData = (unsigned char*)xrtMalloc(bytesInRes);
        if ( imageData == NULL ) {
            xrtFree(ico);
            xl6ResBufFree(&group);
            xl6ResSetError(error, errorSize, "cannot allocate icon image data");
            return 0;
        }
        memcpy(imageData, ico + imageOffset, bytesInRes);
        if ( !xl6ResourceListAdd(resources, XL6_RT_ICON, i + 1u, language, codepage, imageData, bytesInRes) ||
             !xl6ResBufU8(&group, ico[entry + 0u]) ||
             !xl6ResBufU8(&group, ico[entry + 1u]) ||
             !xl6ResBufU8(&group, ico[entry + 2u]) ||
             !xl6ResBufU8(&group, ico[entry + 3u]) ||
             !xl6ResBufU16(&group, xl6ResReadU16(ico, icoSize, entry + 4u)) ||
             !xl6ResBufU16(&group, xl6ResReadU16(ico, icoSize, entry + 6u)) ||
             !xl6ResBufU32(&group, bytesInRes) ||
             !xl6ResBufU16(&group, i + 1u) ) {
            xrtFree(ico);
            xl6ResBufFree(&group);
            xl6ResSetError(error, errorSize, "cannot build icon resources");
            return 0;
        }
    }
    groupData = xl6ResBufTake(&group, &groupSize);
    xrtFree(ico);
    if ( !xl6ResourceListAdd(resources, XL6_RT_GROUP_ICON, 1u, language, codepage, groupData, groupSize) ) {
        xl6ResSetError(error, errorSize, "cannot add group icon resource");
        return 0;
    }
    return 1;
}

static int xl6AddManifestResource(
    XL6_WinResourceList* resources,
    xvalue manifest,
    const char* baseDir,
    unsigned int language,
    unsigned int codepage,
    char* error,
    size_t errorSize)
{
    unsigned char* data;
    size_t size;

    if ( manifest == NULL || xvoIsNull(manifest) ) {
        return 1;
    }
    data = NULL;
    size = 0;
    if ( !xl6ReadBinarySpec(manifest, baseDir, 1, &data, &size, error, errorSize) ) {
        return 0;
    }
    if ( size == 0 ) {
        xrtFree(data);
        xl6ResSetError(error, errorSize, "manifest resource is empty");
        return 0;
    }
    if ( !xl6ResourceListAdd(resources, XL6_RT_MANIFEST, 1u, language, codepage, data, size) ) {
        xl6ResSetError(error, errorSize, "cannot add manifest resource");
        return 0;
    }
    return 1;
}

static size_t xl6WriteResourceDirectory(XL6_ResBuf* buf, unsigned int idCount)
{
    size_t off;

    off = buf->size;
    if ( !xl6ResBufU32(buf, 0u) ||
         !xl6ResBufU32(buf, 0u) ||
         !xl6ResBufU16(buf, 0u) ||
         !xl6ResBufU16(buf, 0u) ||
         !xl6ResBufU16(buf, 0u) ||
         !xl6ResBufU16(buf, idCount) ||
         !xl6ResBufZero(buf, (size_t)idCount * 8u) ) {
        return (size_t)-1;
    }
    return off;
}

static void xl6PatchResourceDirectoryEntry(
    XL6_ResBuf* buf,
    size_t dirOffset,
    unsigned int index,
    unsigned int id,
    size_t targetOffset,
    int targetIsDir)
{
    size_t entry = dirOffset + 16u + (size_t)index * 8u;

    xl6ResBufPatchU32(buf, entry, id);
    xl6ResBufPatchU32(buf, entry + 4u, (unsigned int)targetOffset | (targetIsDir ? 0x80000000u : 0u));
}

static size_t xl6CountTypes(const XL6_WinResourceList* resources)
{
    size_t count;
    size_t i;

    count = 0;
    for ( i = 0; resources != NULL && i < resources->count; ++i ) {
        if ( i == 0 || resources->items[i].typeId != resources->items[i - 1u].typeId ) {
            count += 1u;
        }
    }
    return count;
}

static size_t xl6CountNames(const XL6_WinResourceList* resources, size_t first, size_t last)
{
    size_t count;
    size_t i;

    count = 0;
    for ( i = first; i < last; ++i ) {
        if ( i == first || resources->items[i].nameId != resources->items[i - 1u].nameId ) {
            count += 1u;
        }
    }
    return count;
}

static int xl6BuildRsrcSection(
    XL6_WinResourceList* resources,
    XL6_ResBuf* out,
    XL6_RelocList* relocs,
    char* error,
    size_t errorSize)
{
    XL6_DataEntryFixupList fixups;
    size_t root;
    size_t typeIndex;
    size_t i;

    if ( resources == NULL || resources->count == 0 ) {
        xl6ResSetError(error, errorSize, "windows resource config does not define any resources");
        return 0;
    }
    qsort(resources->items, resources->count, sizeof(resources->items[0]), xl6ResourceCompare);
    memset(&fixups, 0, sizeof(fixups));
    root = xl6WriteResourceDirectory(out, (unsigned int)xl6CountTypes(resources));
    if ( root == (size_t)-1 ) {
        xl6ResSetError(error, errorSize, "cannot create resource root directory");
        return 0;
    }
    typeIndex = 0;
    i = 0;
    while ( i < resources->count ) {
        size_t typeFirst = i;
        size_t typeLast;
        size_t typeDir;
        size_t nameIndex;

        while ( i < resources->count && resources->items[i].typeId == resources->items[typeFirst].typeId ) {
            i += 1u;
        }
        typeLast = i;
        typeDir = xl6WriteResourceDirectory(out, (unsigned int)xl6CountNames(resources, typeFirst, typeLast));
        if ( typeDir == (size_t)-1 ) {
            xl6FixupFree(&fixups);
            xl6ResSetError(error, errorSize, "cannot create resource type directory");
            return 0;
        }
        xl6PatchResourceDirectoryEntry(out, root, (unsigned int)typeIndex, resources->items[typeFirst].typeId, typeDir, 1);
        typeIndex += 1u;

        nameIndex = 0;
        for ( i = typeFirst; i < typeLast; ) {
            size_t nameFirst = i;
            size_t nameLast;
            size_t nameDir;
            size_t langIndex;

            while ( i < typeLast && resources->items[i].nameId == resources->items[nameFirst].nameId ) {
                i += 1u;
            }
            nameLast = i;
            nameDir = xl6WriteResourceDirectory(out, (unsigned int)(nameLast - nameFirst));
            if ( nameDir == (size_t)-1 ) {
                xl6FixupFree(&fixups);
                xl6ResSetError(error, errorSize, "cannot create resource name directory");
                return 0;
            }
            xl6PatchResourceDirectoryEntry(out, typeDir, (unsigned int)nameIndex, resources->items[nameFirst].nameId, nameDir, 1);
            nameIndex += 1u;

            for ( langIndex = 0; nameFirst + langIndex < nameLast; ++langIndex ) {
                size_t itemIndex = nameFirst + langIndex;
                size_t dataEntry = out->size;

                if ( !xl6ResBufU32(out, 0u) ||
                     !xl6ResBufU32(out, (unsigned int)resources->items[itemIndex].size) ||
                     !xl6ResBufU32(out, resources->items[itemIndex].codepage) ||
                     !xl6ResBufU32(out, 0u) ||
                     !xl6RelocAppend(relocs, (unsigned int)dataEntry) ||
                     !xl6FixupAppend(&fixups, dataEntry, itemIndex) ) {
                    xl6FixupFree(&fixups);
                    xl6ResSetError(error, errorSize, "cannot create resource data entry");
                    return 0;
                }
                xl6PatchResourceDirectoryEntry(out, nameDir, (unsigned int)langIndex, resources->items[itemIndex].langId, dataEntry, 0);
            }
        }
        i = typeLast;
    }
    for ( i = 0; i < fixups.count; ++i ) {
        XL6_WinResourceItem* item = &resources->items[fixups.items[i].itemIndex];
        size_t dataOffset;

        if ( !xl6ResBufAlign4(out) ) {
            xl6FixupFree(&fixups);
            xl6ResSetError(error, errorSize, "cannot align resource data");
            return 0;
        }
        dataOffset = out->size;
        if ( dataOffset > 0xFFFFFFFFu ||
             !xl6ResBufWrite(out, item->data, item->size) ) {
            xl6FixupFree(&fixups);
            xl6ResSetError(error, errorSize, "cannot append resource data");
            return 0;
        }
        xl6ResBufPatchU32(out, fixups.items[i].dataEntryOffset, (unsigned int)dataOffset);
    }
    xl6FixupFree(&fixups);
    return xl6ResBufAlign4(out);
}

static int xl6BuildCoffObject(
    const XL6_ResBuf* rsrc,
    const XL6_RelocList* relocs,
    XL6_ResBuf* out,
    char* error,
    size_t errorSize)
{
    size_t rawOffset;
    size_t relocOffset;
    size_t i;

    if ( rsrc == NULL || rsrc->data == NULL || rsrc->size == 0 ) {
        xl6ResSetError(error, errorSize, "empty resource section");
        return 0;
    }
    if ( rsrc->size > 0xFFFFFFFFu || relocs->count > 0xFFFFu ) {
        xl6ResSetError(error, errorSize, "resource object is too large");
        return 0;
    }
    rawOffset = 20u + 40u;
    relocOffset = rawOffset + rsrc->size;

    if ( !xl6ResBufU16(out, XL6_COFF_MACHINE) ||
         !xl6ResBufU16(out, 1u) ||
         !xl6ResBufU32(out, 0u) ||
         !xl6ResBufU32(out, 0u) ||
         !xl6ResBufU32(out, 0u) ||
         !xl6ResBufU16(out, 0u) ||
         !xl6ResBufU16(out, 0u) ) {
        xl6ResSetError(error, errorSize, "cannot write COFF file header");
        return 0;
    }

    if ( !xl6ResBufWrite(out, ".rsrc\0\0", 8u) ||
         !xl6ResBufU32(out, (unsigned int)rsrc->size) ||
         !xl6ResBufU32(out, 0u) ||
         !xl6ResBufU32(out, (unsigned int)rsrc->size) ||
         !xl6ResBufU32(out, (unsigned int)rawOffset) ||
         !xl6ResBufU32(out, (unsigned int)relocOffset) ||
         !xl6ResBufU32(out, 0u) ||
         !xl6ResBufU16(out, (unsigned int)relocs->count) ||
         !xl6ResBufU16(out, 0u) ||
         !xl6ResBufU32(out, 0x40000040u) ||
         !xl6ResBufWrite(out, rsrc->data, rsrc->size) ) {
        xl6ResSetError(error, errorSize, "cannot write COFF .rsrc section");
        return 0;
    }

    for ( i = 0; i < relocs->count; ++i ) {
        if ( !xl6ResBufU32(out, relocs->offsets[i]) ||
             !xl6ResBufU32(out, 0u) ||
             !xl6ResBufU16(out, XL6_COFF_RSRC_RELOC) ) {
            xl6ResSetError(error, errorSize, "cannot write COFF resource relocation");
            return 0;
        }
    }
    return 1;
}

static xvalue xl6ParseResourceConfig(const char* configPath, char** outText, char* error, size_t errorSize)
{
    char* text;
    size_t size;
    size_t offset;
    xvalue root;

    if ( outText != NULL ) {
        *outText = NULL;
    }
    if ( configPath == NULL || configPath[0] == '\0' ) {
        xl6ResSetError(error, errorSize, "resource config path is empty");
        return NULL;
    }
    size = 0;
    text = (char*)xrtFileReadAll((str)configPath, XRT_CP_UTF8, &size);
    if ( text == NULL ) {
        xl6ResSetError(error, errorSize, "cannot read resource config: %s", configPath);
        return NULL;
    }
    offset = size >= 3u &&
             (unsigned char)text[0] == 0xEFu &&
             (unsigned char)text[1] == 0xBBu &&
             (unsigned char)text[2] == 0xBFu ? 3u : 0u;
    root = xrtJsonValid(text + offset, size - offset) ?
           xrtParseJSON(text + offset, size - offset) :
           NULL;
    if ( root == NULL || xvoIsNull(root) ) {
        xvoUnref(root);
        root = xrtParseXSON(text + offset, size - offset);
    }
    if ( root == NULL || xvoIsNull(root) || !xvoIsTable(root) ) {
        xvoUnref(root);
        xrtFree(text);
        xl6ResSetError(error, errorSize, "resource config must be a JSON/XSON object");
        return NULL;
    }
    if ( outText != NULL ) {
        *outText = text;
    } else {
        xrtFree(text);
    }
    return root;
}

int xl6WinResourceConfigToCoffMemory(
    const char* configPath,
    void** outData,
    size_t* outSize,
    char* error,
    size_t errorSize)
{
    char* configText;
    char* baseDir;
    xvalue root;
    XL6_WinResourceList resources;
    XL6_ResBuf rsrc;
    XL6_RelocList relocs;
    XL6_ResBuf coff;
    unsigned int language;
    unsigned int codepage;
    int ok;

    if ( outData == NULL || outSize == NULL ) {
        xl6ResSetError(error, errorSize, "resource output pointer is empty");
        return 0;
    }
    *outData = NULL;
    *outSize = 0;
    memset(&resources, 0, sizeof(resources));
    memset(&rsrc, 0, sizeof(rsrc));
    memset(&relocs, 0, sizeof(relocs));
    memset(&coff, 0, sizeof(coff));
    configText = NULL;
    root = xl6ParseResourceConfig(configPath, &configText, error, errorSize);
    if ( root == NULL ) {
        return 0;
    }
    baseDir = (char*)xrtPathGetDir((str)configPath, 0);
    if ( baseDir == NULL || baseDir[0] == '\0' ) {
        xrtFree(baseDir);
        baseDir = (char*)xrtCopyStr((str)".", 0);
    }
    language = xl6UIntField(root, "language", 1033u);
    codepage = xl6UIntField(root, "codepage", 1200u);

    ok = xl6AddVersionResource(&resources, xl6Field(root, "version"), language, codepage, error, errorSize) &&
         xl6AddIconResources(&resources, xl6Field(root, "icon"), baseDir, language, codepage, error, errorSize) &&
         xl6AddManifestResource(&resources, xl6Field(root, "manifest"), baseDir, language, codepage, error, errorSize) &&
         xl6BuildRsrcSection(&resources, &rsrc, &relocs, error, errorSize) &&
         xl6BuildCoffObject(&rsrc, &relocs, &coff, error, errorSize);
    if ( ok ) {
        *outData = xl6ResBufTake(&coff, outSize);
        ok = *outData != NULL && *outSize != 0;
        if ( !ok ) {
            xl6ResSetError(error, errorSize, "cannot take generated resource object");
        }
    }
    xl6ResBufFree(&coff);
    xl6RelocFree(&relocs);
    xl6ResBufFree(&rsrc);
    xl6ResourceListFree(&resources);
    xvoUnref(root);
    xrtFree(configText);
    xrtFree(baseDir);
    return ok;
}

#else

int xl6WinResourceConfigToCoffMemory(
    const char* configPath,
    void** outData,
    size_t* outSize,
    char* error,
    size_t errorSize)
{
    (void)configPath;
    if ( outData != NULL ) {
        *outData = NULL;
    }
    if ( outSize != NULL ) {
        *outSize = 0;
    }
    if ( error != NULL && errorSize != 0 ) {
        snprintf(error, errorSize, "windows resources are only supported on Windows");
        error[errorSize - 1] = '\0';
    }
    return 0;
}

#endif
