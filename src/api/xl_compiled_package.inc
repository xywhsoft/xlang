/* Included only by the owning API unit: registration shares its lifecycle
 * lock and error channel, and never owns language values or module handles. */
#include "src/package/xl6_compiled_package.h"
static void xlClearLastErrorUnlocked(XL_Context*);

static void xlCompiledPackageFree(XL_CompiledPackage* package)
{
    if (!package) return;
    for (size_t i = 0; i < package->entryCount; ++i)
        xrtFree((void*)package->entries[i].name);
    xrtFree((void*)package->entries);
    xrtFree((void*)package->dependencies);
    xrtFree((void*)package->interfaceData);
    xrtFree((void*)package->name);
    xrtFree((void*)package->packageVersion);
    xrtFree((void*)package->abiVersion);
    memset(package, 0, sizeof(*package));
}

static void xlCompiledPackagesFree(XL6_CompiledPackageSet* set)
{
    for (size_t i = 0; i < set->count; ++i) xlCompiledPackageFree(&set->items[i]);
    xrtFree(set->items);
    memset(set, 0, sizeof(*set));
}

const void* xlCompiledPackageRuntimeIdentity(void)
{
    return (const void*)xrtValueRelease;
}

uint64_t xlCompiledPackageInterfaceHash(const void* data, size_t size)
{
    const unsigned char* bytes = (const unsigned char*)data;
    uint64_t hash = UINT64_C(14695981039346656037);
    if (!bytes && size) return 0;
    for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
    return hash;
}

static const XL_CompiledPackage* xlCompiledPackageFind(
    const XL6_CompiledPackageSet* set, const char* name)
{
    for (size_t i = 0; set && i < set->count; ++i)
        if (!strcmp(set->items[i].name, name)) return &set->items[i];
    return NULL;
}

static int xlCompiledPackageNameValid(const char* text)
{
    size_t length = 0;
    if (!text || !text[0]) return 0;
    if (!(text[0]=='_' || (text[0]>='a' && text[0]<='z') || (text[0]>='A' && text[0]<='Z'))) return 0;
    for (; text[length]; ++length) {
        unsigned char c = (unsigned char)text[length];
        if (length >= 255 || !(c == '_' || c == '.' || c == '-' ||
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (length && c >= '0' && c <= '9'))) return 0;
    }
    return 1;
}

static int xlCompiledPackageSymbolValid(const char* text)
{
    if (!xlCompiledPackageNameValid(text)) return 0;
    for (; *text; ++text) if (*text == '.' || *text == '-') return 0;
    return 1;
}

static int xlCompiledPackageVersionValid(const char* text)
{
    if (!text || !*text) return 0;
    for (size_t i = 0; text[i]; ++i)
        if (i >= 255 || (unsigned char)text[i] < 33 || (unsigned char)text[i] > 126) return 0;
    return 1;
}

static int xlCompiledPackageEntryCompare(const void* a,const void* b)
{
    return strcmp(((const XL_PackageEntry*)a)->name,((const XL_PackageEntry*)b)->name);
}

static const XL_PackageEntry* xlCompiledPackageEntryFind(const XL_CompiledPackage* package,const char* name)
{
    size_t low=0,high=package->entryCount;
    while(low<high) {
        size_t middle=low+(high-low)/2;
        int order=strcmp(name,package->entries[middle].name);
        if(!order)return &package->entries[middle];
        if(order<0)high=middle;else low=middle+1;
    }
    return NULL;
}

static int xlCompiledPackageEqual(const XL_CompiledPackage* a, const XL_CompiledPackage* b,
    const XL_CompiledPackage* aIdentity,const XL_CompiledPackage* bIdentity)
{
    if (strcmp(a->name, b->name) || strcmp(a->packageVersion, b->packageVersion) ||
        strcmp(a->abiVersion, b->abiVersion) || a->interfaceSize != b->interfaceSize ||
        a->interfaceHash != b->interfaceHash || a->entryCount != b->entryCount ||
        aIdentity->dependencyCount != bIdentity->dependencyCount ||
        memcmp(a->interfaceData, b->interfaceData, a->interfaceSize)) return 0;
    for (size_t i = 0; i < a->entryCount; ++i)
        if (strcmp(a->entries[i].name, b->entries[i].name) ||
            a->entries[i].address != b->entries[i].address) return 0;
    for (size_t i = 0; i < aIdentity->dependencyCount; ++i)
        if (strcmp(aIdentity->dependencies[i]->name,bIdentity->dependencies[i]->name) ||
            strcmp(aIdentity->dependencies[i]->packageVersion,bIdentity->dependencies[i]->packageVersion) ||
            strcmp(aIdentity->dependencies[i]->abiVersion,bIdentity->dependencies[i]->abiVersion) ||
            aIdentity->dependencies[i]->interfaceHash!=bIdentity->dependencies[i]->interfaceHash) return 0;
    return 1;
}

static int xlCompiledPackageCopy(XL_CompiledPackage* out, const XL_CompiledPackage* in)
{
    memset(out, 0, sizeof(*out));
    *out = *in;
    out->name = out->packageVersion = out->abiVersion = NULL;
    out->interfaceData = NULL; out->entries = NULL; out->entryCount = 0;
    /* The flattened set owns dependencies independently; never retain caller
     * descriptor pointers after this transaction. */
    out->dependencies = NULL; out->dependencyCount = 0;
    out->name = xrtStrDup(in->name);
    out->packageVersion = xrtStrDup(in->packageVersion);
    out->abiVersion = xrtStrDup(in->abiVersion);
    out->interfaceData = (const unsigned char*)xrtMalloc(in->interfaceSize);
    if (!out->name || !out->packageVersion || !out->abiVersion || !out->interfaceData) goto failed;
    memcpy((void*)out->interfaceData, in->interfaceData, in->interfaceSize);
    if (in->entryCount) {
        XL_PackageEntry* entries = (XL_PackageEntry*)xrtCalloc(in->entryCount, sizeof(*entries));
        if (!entries) goto failed;
        out->entries = entries;
        for (size_t i = 0; i < in->entryCount; ++i) {
            entries[i].address = in->entries[i].address;
            entries[i].name = xrtStrDup(in->entries[i].name);
            out->entryCount = i + 1;
            if (!entries[i].name) goto failed;
        }
        qsort(entries,in->entryCount,sizeof(*entries),xlCompiledPackageEntryCompare);
        for(size_t i=1;i<in->entryCount;++i)
            if(!strcmp(entries[i-1].name,entries[i].name))goto failed;
    }
    return 1;
failed:
    xlCompiledPackageFree(out);
    return 0;
}

typedef struct XL_CompiledPackageImport {
    XL_Context* ctx;
    XL6_CompiledPackageSet staged;
    const XL_CompiledPackage* active[XL_COMPILED_PACKAGE_MAX_PACKAGES];
    const XL_CompiledPackage* origins[XL_COMPILED_PACKAGE_MAX_PACKAGES];
    size_t depth;
    size_t entries, bytes;
} XL_CompiledPackageImport;

static int xlCompiledPackageStage(XL_CompiledPackageImport* tx, const XL_CompiledPackage* in)
{
    const XL_CompiledPackage *previous,*previousIdentity;
    XL_CompiledPackage copy, *items;
    if (!in || in->size != sizeof(*in) || in->version != XL_COMPILED_PACKAGE_VERSION ||
        in->pointerBytes != sizeof(void*) || in->compilerAbi != XL_COMPILED_PACKAGE_COMPILER_ABI ||
        in->runtimeIdentity != xlCompiledPackageRuntimeIdentity() ||
        !xlCompiledPackageNameValid(in->name) || !xlCompiledPackageVersionValid(in->packageVersion) ||
        !xlCompiledPackageVersionValid(in->abiVersion) ||
        !in->interfaceData || !in->interfaceSize || in->interfaceSize > XL_COMPILED_PACKAGE_MAX_INTERFACE ||
        in->entryCount > XL_COMPILED_PACKAGE_MAX_ENTRIES || (in->entryCount && !in->entries) ||
        in->dependencyCount > XL_COMPILED_PACKAGE_MAX_PACKAGES || (in->dependencyCount && !in->dependencies) ||
        xlCompiledPackageInterfaceHash(in->interfaceData, in->interfaceSize) != in->interfaceHash ||
        !xl6CompiledPackageInterfaceValid(in->interfaceData, in->interfaceSize)) {
        xlSetError(tx->ctx, "invalid compiled package descriptor, interface or ABI"); return 0;
    }
    if (!strcmp(in->name,"xlang") || !strcmp(in->name,"tcc") ||
        !strcmp(in->name,"xlang.compiler") || !strcmp(in->name,"concurrent")) {
        xlSetError(tx->ctx, "compiled package name is reserved"); return 0;
    }
    for (size_t i = 0; i < in->entryCount; ++i) {
        if (!xlCompiledPackageSymbolValid(in->entries[i].name) || !in->entries[i].address) {
            xlSetError(tx->ctx, "invalid compiled package native entry"); return 0;
        }
    }
    for (size_t i = 0; i < tx->depth; ++i)
        if (!strcmp(tx->active[i]->name, in->name)) {
            xlSetError2(tx->ctx, "compiled package dependency cycle", in->name); return 0;
        }
    for(size_t p=0;p<tx->staged.count;++p)
        if(tx->origins[p]==in)return 1;
    if (tx->depth == XL_COMPILED_PACKAGE_MAX_PACKAGES) {
        xlSetError(tx->ctx, "compiled package registration limit exceeded"); return 0;
    }
    if (!xlCompiledPackageCopy(&copy, in)) {
        xlSetError(tx->ctx, "cannot copy compiled package or duplicate native entries"); return 0;
    }
    tx->active[tx->depth++] = in;
    for (size_t i = 0; i < in->dependencyCount; ++i)
        if (!xlCompiledPackageStage(tx, in->dependencies[i])) {
            --tx->depth;xlCompiledPackageFree(&copy);return 0;
        }
    --tx->depth;
    previous = xlCompiledPackageFind(&tx->ctx->compiledPackages, in->name);
    previousIdentity=previous;
    if (!previous) for(size_t p=0;p<tx->staged.count;++p)
        if(!strcmp(tx->staged.items[p].name,in->name)) {
            previous=&tx->staged.items[p];previousIdentity=tx->origins[p];break;
        }
    if (previous) {
        int equal=xlCompiledPackageEqual(previous, &copy, previousIdentity,in);
        xlCompiledPackageFree(&copy);
        if (!equal) {
            xlSetError2(tx->ctx, "compiled package identity conflict", in->name); return 0;
        }
        return 1;
    }
    if (tx->ctx->compiledPackages.count + tx->staged.count >= XL_COMPILED_PACKAGE_MAX_PACKAGES ||
        tx->entries > XL_COMPILED_PACKAGE_MAX_ENTRIES - in->entryCount ||
        tx->bytes > XL_COMPILED_PACKAGE_MAX_INTERFACE - in->interfaceSize) {
        xlCompiledPackageFree(&copy);
        xlSetError(tx->ctx,"compiled package registration limit exceeded");return 0;
    }
    items = (XL_CompiledPackage*)xrtRealloc(tx->staged.items,
        (tx->staged.count + 1) * sizeof(*items));
    if (!items) { xlCompiledPackageFree(&copy); xlSetError(tx->ctx, "cannot stage compiled package"); return 0; }
    tx->staged.items = items; tx->origins[tx->staged.count]=in;
    items[tx->staged.count++] = copy;
    tx->entries += in->entryCount; tx->bytes += in->interfaceSize;
    return 1;
}

static int xlCompiledPackageDefaultBinding(void* data,const char* name,const void* address)
{
    XL_CompiledPackageImport* tx=(XL_CompiledPackageImport*)data;
    for(size_t p=0;p<tx->staged.count;++p) {
        const XL_PackageEntry* entry=xlCompiledPackageEntryFind(&tx->staged.items[p],name);
        if(entry && entry->address!=address) {
            xlSetError2(tx->ctx,"compiled package cannot override a default runtime symbol",name);return 0;
        }
    }
    return 1;
}

int xlImportCompiledPackage(XL_Context* ctx, const XL_CompiledPackage* package)
{
    XL_CompiledPackageImport tx = {0};
    XL_CompiledPackage* items;
    int ok = 0;
    if (!ctx) return 0;
    xlApiLock();
    xlClearLastErrorUnlocked(ctx);
    tx.ctx = ctx;
    if (ctx->destroyRequested) { xlSetError(ctx, "context is closing"); goto done; }
    for (size_t p = 0; p < ctx->compiledPackages.count; ++p) {
        tx.entries += ctx->compiledPackages.items[p].entryCount;
        tx.bytes += ctx->compiledPackages.items[p].interfaceSize;
    }
    if (!xlCompiledPackageStage(&tx, package)) goto done;
    if(!xlApiVisitDefaultRuntimeSymbols(xlCompiledPackageDefaultBinding,&tx))goto done;
    /* Check every native binding before publishing any metadata. */
    for (size_t p = 0; p < tx.staged.count; ++p) {
        const XL_CompiledPackage* candidate = &tx.staged.items[p];
        for (size_t e = 0; e < candidate->entryCount; ++e) {
            const XL_PackageEntry* entry = &candidate->entries[e];
            for (size_t s = 0; s < ctx->symbolCount; ++s)
                if (!strcmp(entry->name, ctx->symbols[s].name) && entry->address != ctx->symbols[s].value) {
                    xlSetError2(ctx, "compiled package native symbol conflict", entry->name); goto done;
                }
            for (size_t q = 0; q < ctx->compiledPackages.count + p; ++q) {
                const XL_CompiledPackage* other = q < ctx->compiledPackages.count
                    ? &ctx->compiledPackages.items[q] : &tx.staged.items[q - ctx->compiledPackages.count];
                const XL_PackageEntry* found=xlCompiledPackageEntryFind(other,entry->name);
                if(found && found->address!=entry->address) {
                    xlSetError2(ctx,"compiled package native symbol conflict",entry->name);goto done;
                }
            }
        }
    }
    if (!tx.staged.count) { ok = 1; goto done; }
    items = ctx->compiledPackages.items;
    if(!items)items=(XL_CompiledPackage*)xrtCalloc(XL_COMPILED_PACKAGE_MAX_PACKAGES,sizeof(*items));
    if (!items) { xlSetError(ctx, "cannot publish compiled packages"); goto done; }
    /* Allocate all dependency vectors before publication; target records have
     * stable addresses throughout this context's lifetime. */
    for(size_t p=0;p<tx.staged.count;++p) {
        const XL_CompiledPackage* origin=tx.origins[p];
        if(!origin->dependencyCount)continue;
        const XL_CompiledPackage** dependencies=(const XL_CompiledPackage**)xrtCalloc(
            origin->dependencyCount,sizeof(*dependencies));
        if(!dependencies) {
            if(!ctx->compiledPackages.items)xrtFree(items);
            xlSetError(ctx,"cannot publish compiled package dependencies");goto done;
        }
        tx.staged.items[p].dependencies=dependencies;
        tx.staged.items[p].dependencyCount=origin->dependencyCount;
        for(size_t d=0;d<origin->dependencyCount;++d) {
            dependencies[d]=xlCompiledPackageFind(&ctx->compiledPackages,origin->dependencies[d]->name);
            if(!dependencies[d])for(size_t q=0;q<tx.staged.count;++q)
                if(!strcmp(tx.staged.items[q].name,origin->dependencies[d]->name)) {
                    dependencies[d]=&items[ctx->compiledPackages.count+q];break;
                }
            if(!dependencies[d])abort();
        }
    }
    memcpy(items + ctx->compiledPackages.count, tx.staged.items, tx.staged.count * sizeof(*items));
    ctx->compiledPackages.items = items; ctx->compiledPackages.count += tx.staged.count;
    xrtFree(tx.staged.items); memset(&tx.staged, 0, sizeof(tx.staged)); ok = 1;
done:
    xlCompiledPackagesFree(&tx.staged);
    xlApiUnlock();
    return ok;
}

const XL_CompiledPackage* xlFindCompiledPackage(XL_Context* ctx, const char* name)
{
    const XL_CompiledPackage* result;
    if (!ctx || !name) return NULL;
    xlApiLock(); result = xlCompiledPackageFind(&ctx->compiledPackages, name); xlApiUnlock();
    return result;
}
