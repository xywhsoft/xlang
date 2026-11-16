/* Registered interfaces keep immutable selected capabilities. Their explicit
 * native dependency closure is independent of their wider declaration imports.
 */
static const XL_CompiledPackage* xl6ImportsFindCompiledPackage(const XL6_ImportDeps* deps, const char* name)
{
    for (size_t p = 0; deps && deps->compiledPackages && p < deps->compiledPackages->count; ++p)
        if (!strcmp(name, deps->compiledPackages->items[p].name)) return &deps->compiledPackages->items[p];
    return NULL;
}

static int xl6ImportsCollectCompiledFeatures(XL6_ImportDeps* deps, const XL_CompiledPackage* package,
    const XL6_AstNode* importNode, XL6_DiagList* diags, unsigned depth)
{
    if (depth > XL_COMPILED_PACKAGE_MAX_PACKAGES) return 0;
    for (size_t p = 0; p < deps->packageInfoCount; ++p) {
        const XL6_ImportPackageInfo* existing = &deps->packageInfos[p];
        if (strcmp(existing->name, package->name)) continue;
        if (existing->dir || strcmp(existing->version, package->packageVersion) ||
            strcmp(existing->abiVersion, package->abiVersion)) {
            xl6DiagAdd(diags, 1, 1, "source/compiled package capability identity conflict"); return 0;
        }
        return 1;
    }
    XL6_CompiledNativeFeatures metadata = {0};
    if (!xl6CompiledPackageFeaturesDecode(package, &metadata)) return 0;
    XL6_ImportPackageInfo info = {0};
    info.name = metadata.name; info.version = metadata.version; info.abiVersion = metadata.abiVersion;
    info.dependencies = metadata.dependencies; info.nativeDependencyCount = metadata.dependencies.count;
    info.requiredNativeFeatures = metadata.required; info.providedNativeFeatures = metadata.provided;
    if (info.requiredNativeFeatures.count && importNode) {
        const char* source = xl6AstSourcePath(importNode);
        info.featureSource = source ? xrtStrDup(source) : NULL;
        if (source && !info.featureSource) { xl6CompiledNativeFeaturesFree(&metadata); return 0; }
        info.featureLine = importNode->line; info.featureColumn = importNode->column;
    }
    XL6_ImportPackageInfo* items = (XL6_ImportPackageInfo*)xrtRealloc(
        deps->packageInfos, (deps->packageInfoCount + 1) * sizeof(*items));
    if (!items) { xrtFree(info.featureSource); xl6CompiledNativeFeaturesFree(&metadata); return 0; }
    deps->packageInfos = items; items[deps->packageInfoCount++] = info;
    /* A recursive insertion can move items. Metadata's owned string arrays
     * stay stable until the complete import transaction is freed. */
    for (size_t d = 0; d < metadata.dependencies.count; ++d) {
        const char* name = metadata.dependencies.items[d];
        const XL_CompiledPackage* child = xl6ImportsFindCompiledPackage(deps, name);
        if (!child) {
            xl6DiagAddFile(diags, xl6AstSourcePath(importNode), importNode ? importNode->line : 1,
                importNode ? importNode->column : 1,
                "compiled package dependency is not registered; filesystem fallback is forbidden");
            return 0;
        }
        const XL_CompiledPackage* declared = NULL;
        for (size_t i = 0; i < package->dependencyCount; ++i)
            if (package->dependencies[i] && !strcmp(name, package->dependencies[i]->name)) {
                declared = package->dependencies[i]; break;
            }
        if (!declared || strcmp(child->packageVersion, declared->packageVersion) ||
            strcmp(child->abiVersion, declared->abiVersion) || child->interfaceHash != declared->interfaceHash ||
            !xl6ImportsCollectCompiledFeatures(deps, child, importNode, diags, depth + 1)) return 0;
    }
    return 1;
}
