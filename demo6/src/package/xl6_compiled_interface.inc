/* XLP1 is a bounded declaration tree, not a memory image or source archive.
 * Integers are little-endian u32. No pointers or semantic IDs are serialized. */
#include "src/package/xl6_compiled_package.h"

typedef struct XL6_PackageReader {
    const unsigned char* data;
    size_t size, offset, nodes;
    int failed, materialize;
} XL6_PackageReader;

static uint32_t xl6PackageReadU32(XL6_PackageReader* r)
{
    uint32_t value = 0;
    if (r->failed || r->offset > r->size || r->size - r->offset < 4) { r->failed = 1; return 0; }
    for (unsigned i = 0; i < 4; ++i) value |= (uint32_t)r->data[r->offset++] << (8u * i);
    return value;
}

static const unsigned char* xl6PackageReadBytes(XL6_PackageReader* r, size_t size)
{
    const unsigned char* result;
    if (r->failed || r->offset > r->size || size > r->size - r->offset) { r->failed = 1; return NULL; }
    result = r->data + r->offset; r->offset += size; return result;
}

static XL6_AstNode* xl6PackageReadNode(XL6_PackageReader* r, unsigned depth)
{
    uint32_t kind = xl6PackageReadU32(r), flags, textSize, literalSize, childCount;
    uint32_t linkSize, error, scalar, decoded;
    const unsigned char *text, *literal, *link;
    XL6_AstNode* node = NULL;
    if (kind == UINT32_MAX) { r->failed=1;return NULL; }
    if (r->failed || depth > 128 || ++r->nodes > 65536 || kind > XL6_AST_CONTINUE) { r->failed = 1; return NULL; }
    flags = xl6PackageReadU32(r); textSize = xl6PackageReadU32(r);
    literalSize = xl6PackageReadU32(r); childCount = xl6PackageReadU32(r);
    linkSize = xl6PackageReadU32(r); error = xl6PackageReadU32(r);
    scalar = xl6PackageReadU32(r); decoded = xl6PackageReadU32(r);
    if (r->failed || textSize > 65536 || childCount > 65536 || linkSize > 255 ||
        decoded > 1 ||
        ((kind!=XL6_AST_PARAM || !XL6_COMPILED_PARAM_LIFETIME_VALID(error)) &&
         ((error != 0 && error != 1 && error != 2) || ((linkSize == 0) != (error == 0)))) ||
        (linkSize && kind != XL6_AST_FUNC && kind != XL6_AST_GET && kind != XL6_AST_LET && kind != XL6_AST_CREATE)) {
        r->failed = 1; return NULL;
    }
    text = xl6PackageReadBytes(r, textSize); literal = xl6PackageReadBytes(r, literalSize);
    link = xl6PackageReadBytes(r, linkSize);
    if (r->failed || (textSize && memchr(text, 0, textSize)) || (linkSize && memchr(link, 0, linkSize))) {
        r->failed = 1; return NULL;
    }
    for (uint32_t i = 0; i < linkSize; ++i)
        if (!(link[i] == '_' || (link[i] >= 'a' && link[i] <= 'z') ||
            (link[i] >= 'A' && link[i] <= 'Z') || (i && link[i] >= '0' && link[i] <= '9'))) {
            r->failed = 1; return NULL;
        }
    if (r->materialize) {
        node = xl6AstNewText((XL6_AstKind)kind, (const char*)text, textSize, NULL);
        if (!node || !node->text) { xl6AstFree(node);r->failed = 1; return NULL; }
        node->flags = flags; node->literalScalar = scalar; node->literalDecoded = (int)decoded;
        node->compiledPackageError=error;
        if (decoded && !xl6AstSetLiteralData(node, literal, literalSize)) r->failed = 1;
        if (linkSize) {
            node->compiledPackageLink = xrtStrDupN((const char*)link, linkSize);
            node->compiledPackageError = error;
            if (!node->compiledPackageLink) r->failed = 1;
        }
    }
    for (uint32_t i = 0; !r->failed && i < childCount; ++i) {
        XL6_AstNode* child = xl6PackageReadNode(r, depth + 1);
        if (r->materialize && !r->failed) {
            size_t before = node->childCount;
            xl6AstAddChild(node, child);
            if (node->childCount != before + 1) { xl6AstFree(child); r->failed = 1; }
        } else if (r->materialize) xl6AstFree(child);
    }
    if (r->failed) { xl6AstFree(node); return NULL; }
    return node;
}

static XL6_AstNode* xl6CompiledPackageRead(const void* data, size_t size, int materialize, int* valid)
{
    XL6_PackageReader reader = {(const unsigned char*)data, size, 4, 0, 0, materialize};
    XL6_AstNode* result;
    *valid = 0;
    if (!data || size < 40 || size > XL_COMPILED_PACKAGE_MAX_INTERFACE || memcmp(data, "XLP1", 4)) return NULL;
    if (((const unsigned char*)data)[4] != XL6_AST_MODULE ||
        ((const unsigned char*)data)[5] || ((const unsigned char*)data)[6] || ((const unsigned char*)data)[7]) return NULL;
    result = xl6PackageReadNode(&reader, 0);
    *valid = !reader.failed && reader.offset == reader.size && reader.nodes != 0;
    if (!*valid) { xl6AstFree(result); return NULL; }
    return result;
}

int xl6CompiledPackageInterfaceValid(const void* data, size_t size)
{
    int valid;
    (void)xl6CompiledPackageRead(data, size, 0, &valid);
    return valid;
}

XL6_AstNode* xl6CompiledPackageDecode(const void* data, size_t size)
{
    int valid;
    return xl6CompiledPackageRead(data, size, 1, &valid);
}
