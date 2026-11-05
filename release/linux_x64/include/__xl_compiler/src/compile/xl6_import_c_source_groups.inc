static int xl6ImportsGroupListEqual(const XL6_PackageStringList* a,
    const XL6_PackageStringList* b)
{
    if (a->count != b->count) return 0;
    for (size_t i = 0; i < a->count; ++i)
        if (strcmp(a->items[i], b->items[i])) return 0;
    return 1;
}
static int xl6ImportsAppendCSourceGroup(XL6_ImportDeps* deps, const char* directory,
    const XL6_PackageCSourceGroup* input)
{
    char path[4096];
    if (deps->cSourceGroupsResolved ||
        !xl6PackageJoinPath(path, sizeof(path), directory, input->source)) return 0;
    for (size_t i = 0; i < deps->cSourceGroupCount; ++i) {
        const XL6_PackageCSourceGroup* prior = &deps->cSourceGroups[i];
        if (!strcmp(prior->source, path))
            return prior->xrtModulesDeclared == input->xrtModulesDeclared &&
                xl6ImportsGroupListEqual(&prior->providedSymbols, &input->providedSymbols) &&
                xl6ImportsGroupListEqual(&prior->requiredSymbols, &input->requiredSymbols);
        for (size_t a = 0; a < prior->providedSymbols.count; ++a)
            for (size_t b = 0; b < input->providedSymbols.count; ++b)
                if (!strcmp(prior->providedSymbols.items[a], input->providedSymbols.items[b])) return 0;
    }
    XL6_PackageCSourceGroup group = {0};
    group.source = xl6ImportsDupText(path);
    group.xrtModulesDeclared = input->xrtModulesDeclared;
    int ok = group.source != NULL;
    for (size_t i = 0; ok && i < input->providedSymbols.count; ++i)
        ok = xl6PackageStringListAppend(&group.providedSymbols, input->providedSymbols.items[i]);
    for (size_t i = 0; ok && i < input->requiredSymbols.count; ++i)
        ok = xl6PackageStringListAppend(&group.requiredSymbols, input->requiredSymbols.items[i]);
    XL6_PackageCSourceGroup* groups = ok ? (XL6_PackageCSourceGroup*)xrtRealloc(
        deps->cSourceGroups, (deps->cSourceGroupCount + 1) * sizeof(*groups)) : NULL;
    if (!groups) { xl6PackageCSourceGroupFree(&group); return 0; }
    deps->cSourceGroups = groups;groups[deps->cSourceGroupCount++] = group;
    return 1;
}
