#include "src/sem/xl6_sem_index.h"
#include "src/common/xl6_path_identity.h"
#include "src/common/xl6_output.h"
#include "src/type/xl6_type_model.h"
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define XL6_INDEX_NODES 65536u
#define XL6_INDEX_DEPTH 256u
#define XL6_INDEX_BYTES (8u * 1024u * 1024u)

typedef struct XL6_IndexFact {
    const XL6_AstNode* node;
    const XL6_AstNode* selection;
    const XL6_AstNode* type;
    char* canonical;
    unsigned parent, source, selectionSource, declaredType, referencedType;
} XL6_IndexFact;

typedef struct XL6_IndexSource {
    const XL6_AstSource* source;
    unsigned id;
} XL6_IndexSource;

typedef struct XL6_IndexBuild {
    const XL6_AstNode* root;
    const XL6_SourceReport* report;
    XL6_IndexFact* facts;
    XL6_IndexSource* sources;
    size_t count, sourceCount, budget;
    const char* error;
} XL6_IndexBuild;

static int xl6IndexBudget(XL6_IndexBuild* b, size_t bytes)
{
    if (bytes > XL6_INDEX_BYTES - b->budget) {
        b->error = "semantic index exceeds byte limit";
        return 0;
    }
    b->budget += bytes;
    return 1;
}

static int xl6IndexPathEqual(const char* a, const char* b)
{
    return xl6PathIdentityEqual(a, b);
}

static unsigned xl6IndexSourceId(XL6_IndexBuild* b, const XL6_AstNode* node)
{
    const char* path = xl6AstSourcePath(node);
    char* absolute;
    size_t i;
    unsigned id = 0;
    if (!path) return 0;
    for (i = 0; i < b->sourceCount; ++i)
        if (b->sources[i].source == node->source) return b->sources[i].id;
    absolute = xrtPathAbs(path);
    if (!absolute) { b->error = "cannot resolve semantic source path"; return 0; }
    for (i = 0; i < b->report->count; ++i) {
        int equal;
        if ((b->report->items[i].kind & XL6_SOURCE_REPORT_SOURCE) == 0) continue;
        equal = xl6IndexPathEqual(absolute, b->report->items[i].path);
        if (equal < 0) { b->error = "cannot compare semantic source path"; break; }
        if (equal) { id = (unsigned)i + 1; break; }
    }
    xrtFree(absolute);
    /* Generated stdlib nodes may have no loaded file. Their ranges remain null. */
    if (b->sourceCount >= XL6_INDEX_NODES) {
        b->error = "semantic index exceeds source identity limit";
        return 0;
    }
    b->sources[b->sourceCount].source = node->source;
    b->sources[b->sourceCount++].id = id;
    return id;
}

static const XL6_AstNode* xl6IndexSelection(const XL6_AstNode* n)
{
    if (!n->semanticDeclaredSymbolId) return NULL;
    switch (n->kind) {
    case XL6_AST_FUNC: case XL6_AST_ASYNC_FUNC:
    case XL6_AST_EXTERN_FUNC: case XL6_AST_EXPORT_FUNC:
    case XL6_AST_NATIVE_EXTERN_FUNC:
        return n->childCount && n->children[0]->kind == XL6_AST_NAME ? n->children[0] : NULL;
    case XL6_AST_NAME: case XL6_AST_PARAM: case XL6_AST_VAR_DECL: case XL6_AST_CONST_DECL:
    case XL6_AST_CLASS: case XL6_AST_ENUM: case XL6_AST_ENUM_VARIANT: case XL6_AST_UNION:
    case XL6_AST_TYPE_ALIAS: case XL6_AST_EXTERN_TYPE_ALIAS:
        return n;
    default: return NULL; /* No guessed selection for anonymous/implicit entities. */
    }
}

static const char* xl6IndexText(const XL6_AstNode* n)
{
    /* Literal spellings are not symbol names and can fill an entire source. */
    switch (n->kind) {
    case XL6_AST_STRING: case XL6_AST_BYTES: case XL6_AST_CHAR:
    case XL6_AST_INTERP_TEXT: case XL6_AST_INTERP_STRING:
        return NULL;
    default: return n->text;
    }
}

/* These are type-domain facts, independent of executable SymbolIds. Only
 * declaration grammar positions declare an identity; base/constraint NAMEs
 * have IDs supplied by their real semantic checks, never a spelling lookup
 * performed by the serializer. Generic parameters have no concrete ABI. */
static void xl6IndexTypeFacts(XL6_IndexFact* f, const XL6_AstNode* parent)
{
    const XL6_AstNode* n = f->node;
    if (!n->semanticTypeId) return;
    switch (n->kind) {
    case XL6_AST_CLASS: case XL6_AST_ENUM: case XL6_AST_UNION:
    case XL6_AST_TYPE_ALIAS: case XL6_AST_EXTERN_TYPE_ALIAS: case XL6_AST_PROTOCOL:
        f->declaredType = n->semanticTypeId; break;
    case XL6_AST_TYPE:
        f->referencedType = n->semanticTypeId; break;
    case XL6_AST_NAME:
        if (parent && parent->kind == XL6_AST_TYPE_PARAMS)
            f->declaredType = n->semanticTypeId;
        else if (parent && ((parent->kind == XL6_AST_CLASS &&
                   parent->childCount && parent->children[0] == n) ||
                  parent->kind == XL6_AST_GENERIC_CONSTRAINT))
            f->referencedType = n->semanticTypeId;
        break;
    default: break;
    }
    if (f->declaredType && !f->selection) f->selection = n;
}

static int xl6IndexVisit(XL6_IndexBuild* b, const XL6_AstNode* n, unsigned parent, size_t depth)
{
    XL6_IndexFact* f;
    const char* text;
    size_t i, length;
    unsigned id;
    if (!n) return 1;
    if (depth > XL6_INDEX_DEPTH || b->count >= XL6_INDEX_NODES) {
        b->error = "semantic index exceeds tree limit";
        return 0;
    }
    /* Fixed keys, decimal IDs, ABI hashes and three ranges fit this allowance. */
    if (!xl6IndexBudget(b, 1536)) return 0;
    text = xl6IndexText(n);
    length = text ? strlen(text) : 0;
    if (length > XL6_INDEX_BYTES / 6 || !xl6IndexBudget(b, length * 6)) return 0;
    f = &b->facts[b->count++];
    id = (unsigned)b->count;
    f->node = n; f->parent = parent;
    f->source = xl6IndexSourceId(b, n);
    f->selection = xl6IndexSelection(n);
    xl6IndexTypeFacts(f, parent ? b->facts[parent - 1].node : NULL);
    f->selectionSource = f->selection ? xl6IndexSourceId(b, f->selection) : 0;
    if (b->error) return 0;
    f->type = n->semanticDeclaredType ? n->semanticDeclaredType :
              n->inferredType ? n->inferredType : n->kind == XL6_AST_TYPE ? n : NULL;
    if (f->type) {
        f->canonical = xl6TypeToCanonicalStringLimited(b->root, f->type, 8192, 128, 8192);
        if (!f->canonical) { b->error = "canonical semantic type exceeds limits or allocation failed"; return 0; }
        if (!xl6IndexBudget(b, strlen(f->canonical) * 6)) return 0;
    }
    for (i = 0; i < n->childCount; ++i)
        if (!xl6IndexVisit(b, n->children[i], id, depth + 1)) return 0;
    return 1;
}

static void xl6IndexString(XL6_Output* out, const char* s)
{
    const unsigned char* p = (const unsigned char*)s;
    if (!s) { xl6OutputPuts("null", out); return; }
    xl6OutputPutc('"', out);
    for (; *p; ++p) {
        if (*p == '"' || *p == '\\') { xl6OutputPutc('\\', out); xl6OutputPutc(*p, out); }
        else if (*p < 32) xl6OutputPrintf(out, "\\u%04x", (unsigned)*p);
        else xl6OutputPutc(*p, out);
    }
    xl6OutputPutc('"', out);
}

static void xl6IndexRange(XL6_Output* out, const XL6_SourceReport* report,
    unsigned source, const XL6_AstRange* r)
{
    const XL6_SourceReportItem* file;
    const unsigned char* text;
    size_t size, bom;
    if (!source || !r || !r->known) { xl6OutputPuts("null", out); return; }
    file = &report->items[source - 1];
    text = (const unsigned char*)file->text;
    bom = file->size >= 3 && text[0] == 0xef && text[1] == 0xbb && text[2] == 0xbf ? 3 : 0;
    text += bom; size = file->size - bom;
    if (r->endOffset < r->startOffset || r->endOffset > size ||
        (r->startOffset < size && (text[r->startOffset] & 0xc0) == 0x80) ||
        (r->endOffset < size && (text[r->endOffset] & 0xc0) == 0x80)) {
        xl6OutputPuts("null", out);
        return;
    }
    xl6OutputPrintf(out, "{\"startOffset\":%llu,\"endOffset\":%llu,\"startLine\":%d,\"startColumn\":%d,\"endLine\":%d,\"endColumn\":%d}",
        (unsigned long long)r->startOffset, (unsigned long long)r->endOffset,
        r->startLine, r->startColumn, r->endLine, r->endColumn);
}

char* xl6SemIndexJson(const XL6_AstNode* root, const XL6_SourceReport* report,
    char* error, size_t errorSize)
{
    XL6_IndexBuild b;
    XL6_Output out;
    char* json = NULL;
    size_t i, size = 0;
    memset(&b, 0, sizeof(b));
    b.root = root; b.report = report; b.budget = 1024;
    if (error && errorSize) error[0] = 0;
    if (!root || !report || !report->complete) { b.error = "semantic index requires a complete checked source report"; goto done; }
    b.facts = (XL6_IndexFact*)xrtCalloc(XL6_INDEX_NODES, sizeof(*b.facts));
    b.sources = (XL6_IndexSource*)xrtCalloc(XL6_INDEX_NODES, sizeof(*b.sources));
    if (!b.facts || !b.sources) { b.error = "cannot allocate semantic index"; goto done; }
    if (!xl6IndexVisit(&b, root, 0, 1)) goto done;
    if (!xl6OutputOpenMemory(&out)) { b.error = "cannot allocate semantic index output"; goto done; }
    xl6OutputPuts("{\"schema\":1,\"scope\":\"checked-compilation\",\"complete\":true,\"offsetEncoding\":\"utf-8-after-bom\",\"columnEncoding\":\"unicode-scalar-1-based\",\"rangeConvention\":\"half-open\",\"nodes\":[", &out);
    for (i = 0; i < b.count; ++i) {
        const XL6_IndexFact* f = &b.facts[i];
        const XL6_AstNode* n = f->node;
        if (i) xl6OutputPutc(',', &out);
        xl6OutputPrintf(&out, "{\"id\":%u,\"parent\":%u,\"kind\":", (unsigned)i + 1, f->parent);
        xl6IndexString(&out, xl6AstKindName(n->kind));
        xl6OutputPuts(",\"text\":", &out); xl6IndexString(&out, xl6IndexText(n));
        xl6OutputPrintf(&out, ",\"sourceId\":%u,\"range\":", f->source);
        xl6IndexRange(&out, report, f->source, &n->range);
        xl6OutputPuts(",\"tokenRange\":", &out); xl6IndexRange(&out, report, f->source, &n->tokenRange);
        xl6OutputPrintf(&out, ",\"declaredSymbolId\":%u,\"symbolId\":%u,\"targetSymbolId\":%u,\"compoundOperatorSymbolId\":%u,\"selectionSourceId\":%u,\"selectionRange\":",
            (unsigned)n->semanticDeclaredSymbolId, (unsigned)n->semanticSymbolId,
            (unsigned)n->semanticTargetSymbolId, (unsigned)n->semanticCompoundOperatorSymbolId, f->selectionSource);
        xl6IndexRange(&out, report, f->selectionSource, f->selection ? &f->selection->tokenRange : NULL);
        xl6OutputPrintf(&out, ",\"declaredTypeId\":%u,\"referencedTypeId\":%u,\"typeId\":%u,\"typeAbiId\":\"%016llx\",\"type\":",
            f->declaredType, f->referencedType,
            (unsigned)n->semanticTypeId, (unsigned long long)n->semanticTypeAbiId);
        if (!f->type) xl6OutputPuts("null", &out);
        else {
            xl6OutputPuts("{\"origin\":", &out);
            xl6IndexString(&out, n->semanticDeclaredType ? "declaration" : n->inferredType ? "inferred" : "syntax");
            xl6OutputPrintf(&out, ",\"id\":%u,\"abiId\":\"%016llx\",\"canonical\":",
                (unsigned)f->type->semanticTypeId, (unsigned long long)f->type->semanticTypeAbiId);
            xl6IndexString(&out, f->canonical); xl6OutputPutc('}', &out);
        }
        xl6OutputPutc('}', &out);
    }
    xl6OutputPuts("]}", &out);
    if (!xl6OutputHasFailed(&out)) json = xl6OutputTakeMemoryText(&out, &size);
    xl6OutputDiscard(&out);
    if (!json) b.error = "cannot write semantic index";
    else if (size > XL6_INDEX_BYTES) { xrtFree(json); json = NULL; b.error = "semantic index exceeds output limit"; }
done:
    if (!json && error && errorSize) snprintf(error, errorSize, "%s", b.error ? b.error : "semantic index rejected");
    if (b.facts) for (i = 0; i < b.count; ++i) xrtFree(b.facts[i].canonical);
    xrtFree(b.facts); xrtFree(b.sources);
    return json;
}


/* Binding collection records only declaration identity and AST locations. It
 * does not render types or build the much larger whole semantic index. */
#define XL6_BRIDGE_FACTS 262144u
#define XL6_BRIDGE_VISITS 2097152u
#define XL6_BRIDGE_MAP_BYTES (8u * 1024u * 1024u)
typedef struct XL6_BridgeFact {
    unsigned symbol;
    const XL6_AstNode* node;
    const XL6_AstNode* selection;
} XL6_BridgeFact;
typedef struct XL6_BridgeFacts {
    XL6_BridgeFact* items;
    size_t count, capacity, visits;
} XL6_BridgeFacts;

static int xl6BridgeFactsVisit(XL6_BridgeFacts* facts, const XL6_AstNode* node, size_t depth)
{
    const XL6_AstNode* selection;
    size_t i;
    if (!node) return 1;
    if (depth > 512u || facts->visits++ >= XL6_BRIDGE_VISITS) return 0;
    selection = xl6IndexSelection(node);
    if (selection != NULL) {
        if (facts->count >= XL6_BRIDGE_FACTS) return 0;
        if (facts->count == facts->capacity) {
            size_t capacity = facts->capacity ? facts->capacity * 2 : 1024u;
            XL6_BridgeFact* next = (XL6_BridgeFact*)xrtRealloc(facts->items, capacity * sizeof(*next));
            if (!next) return 0;
            facts->items = next; facts->capacity = capacity;
        }
        facts->items[facts->count++] = (XL6_BridgeFact){
            (unsigned)node->semanticDeclaredSymbolId, node, selection};
    }
    for (i = 0; i < node->childCount; ++i)
        if (!xl6BridgeFactsVisit(facts, node->children[i], depth + 1)) return 0;
    return 1;
}
static int xl6BridgeFactCompare(const void* a, const void* b)
{
    unsigned x = ((const XL6_BridgeFact*)a)->symbol, y = ((const XL6_BridgeFact*)b)->symbol;
    return x < y ? -1 : x > y ? 1 : 0;
}
static const XL6_BridgeFact* xl6BridgeFactFind(const XL6_BridgeFacts* facts, unsigned symbol)
{
    size_t lo = 0, hi = facts->count;
    const XL6_BridgeFact* result;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (facts->items[mid].symbol < symbol) lo = mid + 1;
        else hi = mid;
    }
    if (lo == facts->count || facts->items[lo].symbol != symbol) return NULL;
    result = &facts->items[lo++];
    for (; lo < facts->count && facts->items[lo].symbol == symbol; ++lo)
        if (facts->items[lo].node != result->node) return NULL;
    return result;
}
static const xvalue* xl6BridgeGet(const xvalue* v, const char* name)
{
    return v && xrtValueType(v) == XVALUE_OBJECT ? xrtValueObjectGet(v, xrtStrView(name)) : NULL;
}
static int xl6BridgeRangeValid(const XL6_SourceReport* report, unsigned source, const XL6_AstRange* range)
{
    const XL6_SourceReportItem* file;
    const unsigned char* text;
    size_t size, bom;
    if (!source || source > report->count || !range || !range->known ||
        range->startLine < 1 || range->startColumn < 1 ||
        range->endLine < range->startLine || range->endColumn < 1) return 0;
    file = &report->items[source - 1]; text = (const unsigned char*)file->text;
    bom = file->size >= 3 && text[0] == 0xef && text[1] == 0xbb && text[2] == 0xbf ? 3 : 0;
    text += bom; size = file->size - bom;
    return range->startOffset < range->endOffset && range->endOffset <= size &&
        (text[range->startOffset] & 0xc0) != 0x80 &&
        (range->endOffset == size || (text[range->endOffset] & 0xc0) != 0x80);
}
char* xl6SemCBridgeMapJson(const XL6_AstNode* root, const XL6_SourceReport* report,
    const xvalue* bindings, const char* headerText, char* error, size_t errorSize)
{
    XL6_BridgeFacts facts = {0};
    XL6_IndexBuild sources = {0};
    XL6_Output out;
    char* json = NULL;
    const char* failure = "cannot collect bounded C bridge declaration source map";
    size_t i, size = 0, headerSize = headerText ? strlen(headerText) : 0;
    int opened = 0, complete = 1;
    if (error && errorSize) error[0] = 0;
    if (!root || !report || !report->complete || !headerText || !bindings ||
        xrtValueType(bindings) != XVALUE_ARRAY || xrtValueCount(bindings) > 32768u) goto done;
    if (!xl6BridgeFactsVisit(&facts, root, 1)) goto done;
    if (facts.count) qsort(facts.items, facts.count, sizeof(*facts.items), xl6BridgeFactCompare);
    sources.report = report;
    sources.sources = (XL6_IndexSource*)xrtCalloc(XL6_INDEX_NODES, sizeof(*sources.sources));
    if (!sources.sources || !xl6OutputOpenMemory(&out)) goto done;
    opened = 1;
    /* complete is appended only after all records have been inspected. */
    xl6OutputPuts("{\"schema\":1,\"scope\":\"lowered-public-symbol-source-bindings\","
        "\"offsetEncoding\":\"utf-8-after-bom\",\"columnEncoding\":\"unicode-scalar-1-based\","
        "\"rangeConvention\":\"half-open\",\"records\":[", &out);
    for (i = 0; i < xrtValueCount(bindings); ++i) {
        const xvalue* binding = xrtValueArrayGet(bindings, i);
        xstrview name = {0}, kind = {0};
        int64 symbol = 0, start = 0, end = 0;
        const XL6_BridgeFact* fact;
        unsigned source = 0;
        int mapped = 0, typeBinding = 0;
        if (!xrtValueGetInt(xl6BridgeGet(binding, "symbolId"), &symbol) || symbol <= 0 || symbol > UINT32_MAX ||
            !xrtValueGetInt(xl6BridgeGet(binding, "startByte"), &start) || start < 0 ||
            !xrtValueGetInt(xl6BridgeGet(binding, "endByte"), &end) || end <= start || (uint64)end > headerSize ||
            !xrtValueGetString(xl6BridgeGet(binding, "cName"), &name) || name.Size != (size_t)(end - start) ||
            memchr(name.Data, 0, name.Size) || memcmp(headerText + start, name.Data, name.Size) ||
            !xrtValueGetString(xl6BridgeGet(binding, "kind"), &kind) ||
            (strcmp(kind.Data, "function") && strcmp(kind.Data, "global") && strcmp(kind.Data, "type"))) goto done;
        typeBinding = !strcmp(kind.Data, "type");
        fact = xl6BridgeFactFind(&facts, (unsigned)symbol);
        /* The nominal symbol is fixed by lowered union layout. Structural
         * carriers, erased aliases and shared runtime types have no unique
         * source declaration; never recover one from C/type spelling. */
        if (typeBinding && fact && (fact->node->kind != XL6_AST_UNION ||
                !fact->node->semanticTypeId || !fact->node->semanticTypeAbiId)) fact = NULL;
        if (fact) {
            source = xl6IndexSourceId(&sources, fact->selection);
            if (sources.error) goto done;
            mapped = xl6BridgeRangeValid(report, source, &fact->node->range) &&
                xl6BridgeRangeValid(report, source, &fact->selection->tokenRange) &&
                fact->selection->source == fact->node->source &&
                fact->node->range.startOffset <= fact->selection->tokenRange.startOffset &&
                fact->selection->tokenRange.endOffset <= fact->node->range.endOffset;
        }
        if (!mapped) complete = 0;
        if (i) xl6OutputPutc(',', &out);
        xl6OutputPuts("{\"cName\":", &out); xl6IndexString(&out, name.Data);
        xl6OutputPuts(",\"kind\":", &out); xl6IndexString(&out, kind.Data);
        xl6OutputPrintf(&out, ",\"symbolId\":%llu", (unsigned long long)symbol);
        if (typeBinding)
            xl6OutputPrintf(&out, ",\"typeId\":%u,\"typeAbiId\":\"%016llx\"",
                fact ? fact->node->semanticTypeId : 0,
                (unsigned long long)(fact ? fact->node->semanticTypeAbiId : 0));
        xl6OutputPrintf(&out, ",\"headerRange\":{\"startOffset\":%llu,\"endOffset\":%llu},\"target\":",
            (unsigned long long)start, (unsigned long long)end);
        if (mapped) {
            xl6OutputPrintf(&out, "{\"sourceId\":%u,\"range\":", source);
            xl6IndexRange(&out, report, source, &fact->node->range);
            xl6OutputPuts(",\"selectionRange\":", &out);
            xl6IndexRange(&out, report, source, &fact->selection->tokenRange);
            xl6OutputPutc('}', &out);
        } else xl6OutputPuts("null", &out);
        xl6OutputPutc('}', &out);
        if (xl6OutputHasFailed(&out) || out.buffer.Size > XL6_BRIDGE_MAP_BYTES) goto done;
    }
    xl6OutputPrintf(&out, "],\"complete\":%s}", complete ? "true" : "false");
    if (!xl6OutputHasFailed(&out) && out.buffer.Size <= XL6_BRIDGE_MAP_BYTES)
        json = xl6OutputTakeMemoryText(&out, &size);
 done:
    if (opened) xl6OutputClose(&out);
    xrtFree(facts.items); xrtFree(sources.sources);
    if (!json && error && errorSize) snprintf(error, errorSize, "%s", failure);
    return json;
}
