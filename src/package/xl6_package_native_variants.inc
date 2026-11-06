/* Select native implementation metadata without changing the imported .xl API.
 * The package declares paths; the compiler never guesses an SDK directory. */
static int xl6NativeVariantSameList(const XL6_PackageStringList* a,
                                   const XL6_PackageStringList* b)
{
    if (a->count != b->count) return 0;
    for (size_t i = 0; i < a->count; ++i)
        if (strcmp(a->items[i], b->items[i])) return 0;
    return 1;
}

int xl6PackageSelectNativeVariant(const XL6_PackageManifest* base, int selection,
    XL6_PackageManifest* selected, XL6_SourceReport* report, char* error, size_t errorSize)
{
    int wanted = selection == XL6_NATIVE_SELECT_AUTO_AOT ? 1 :
                 selection == XL6_NATIVE_SELECT_AUTO_JIT ? 2 : selection;
    int strict = selection == XL6_NATIVE_SELECT_STATIC || selection == XL6_NATIVE_SELECT_DYNAMIC;
    const char* relative = wanted == 1 ? base->nativeInfo.staticManifest : base->nativeInfo.dynamicManifest;
    char path[1024];
    if (base->nativeInfo.linkage == wanted) return 0;
    if (!relative) {
        if (strict && base->nativeInfo.linkage) {
            snprintf(error, errorSize, "package %s has no %s native artifact", base->name,
                     wanted == 1 ? "static" : "dynamic");
            return -1;
        }
        return 0; /* Legacy packages retain their declared implementation. */
    }
    if (relative[0] == '/' || relative[0] == '\\' || strchr(relative, ':')) {
        snprintf(error, errorSize, "package %s: native variant must be a relative manifest path", base->name);
        return -1;
    }
    if (!xl6PackageJoinPath(path, sizeof(path), base->dir, relative) ||
        !xl6PackageLoadManifestObserved(path, selected, report)) {
        snprintf(error, errorSize, "package %s: unavailable %s native manifest: %s", base->name,
                 wanted == 1 ? "static" : "dynamic", relative);
        return -1;
    }
    int same = selected->nativeInfo.linkage == wanted &&
        !strcmp(base->packageId, selected->packageId) &&
        xl6NativeVariantSameList(&base->xlSources, &selected->xlSources) &&
        base->dependencyCount == selected->dependencyCount;
    for (size_t i = 0; same && i < base->dependencyCount; ++i) {
        const XL6_PackageDependency *a = &base->dependencies[i], *b = &selected->dependencies[i];
        same = !strcmp(a->packageId, b->packageId);
    }
    if (!same) {
        snprintf(error, errorSize, "package %s: native variant changes package identity, language sources or dependencies", base->name);
        return -1;
    }
    return 1;
}
