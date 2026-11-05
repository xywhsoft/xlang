/* Included after the JSON helpers. All names are exact C linkage identifiers;
 * this metadata neither infers C dependencies nor scans C implementation text. */
static int xl6PackageCSourceSymbol(const char* text)
{
    if (!text || !( (*text >= 'A' && *text <= 'Z') ||
        (*text >= 'a' && *text <= 'z') || *text == '_')) return 0;
    for (++text; *text; ++text)
        if (!( (*text >= 'A' && *text <= 'Z') || (*text >= 'a' && *text <= 'z') ||
            (*text >= '0' && *text <= '9') || *text == '_')) return 0;
    return 1;
}
static int xl6PackageCSourceRelative(const char* text)
{
    if (!text || !*text || *text == '/' || *text == '\\') return 0;
    const char* segment = text;
    for (const char* p = text;; ++p) {
        if (*p == ':') return 0;
        if (*p == '/' || *p == '\\' || !*p) {
            size_t size = (size_t)(p - segment);
            if (!size || (size == 1 && segment[0] == '.') ||
                (size == 2 && segment[0] == '.' && segment[1] == '.')) return 0;
            if (!*p) break;
            segment = p + 1;
        }
    }
    return xl6PackageEndsWith(text, ".c");
}
static int xl6PackageCSourceSymbols(const XL6_PackageStringList* list)
{
    if (!list->count) return 0;
    for (size_t i = 0; i < list->count; ++i) {
        if (!xl6PackageCSourceSymbol(list->items[i])) return 0;
        for (size_t j = 0; j < i; ++j)
            if (!strcmp(list->items[i], list->items[j])) return 0;
    }
    return 1;
}
static int xl6PackageCSourceJsonText(const xvalue* item, const char* field, int array)
{
    const xvalue* value = xl6PackageValueField(item, field);
    if (array && xrtValueType(value) != XVALUE_ARRAY) return 0;
    size_t count = array ? xrtValueCount(value) : 1;
    if (!count) return 0;
    for (size_t i = 0; i < count; ++i) {
        xstrview text;
        const xvalue* part = array ? xrtValueArrayGet(value, i) : value;
        if (!xrtValueGetString(part, &text) || !text.Size || memchr(text.Data, 0, text.Size)) return 0;
    }
    return 1;
}
static int xl6PackageValueReadCSourceGroups(const xvalue* artifact,
    XL6_PackageManifest* manifest)
{
    if (!xl6PackageValueHasField(artifact, "cSourceGroups")) return 1;
    xvalue* array = xl6PackageValueField(artifact, "cSourceGroups");
    if (xrtValueType(array) != XVALUE_ARRAY) return 0;
    for (size_t i = 0; i < xrtValueCount(array); ++i) {
        xvalue* item = xrtValueArrayGet(array, i);
        XL6_PackageCSourceGroup group = {0};
        int ok = xrtValueType(item) == XVALUE_OBJECT &&
            xl6PackageCSourceJsonText(item, "source", 0) &&
            xl6PackageCSourceJsonText(item, "providedSymbols", 1) &&
            xl6PackageCSourceJsonText(item, "requiredSymbols", 1) &&
            xl6PackageValueReadText(item, "source", &group.source, 1) &&
            xl6PackageCSourceRelative(group.source) &&
            xl6PackageValueReadStringArray(item, "providedSymbols", &group.providedSymbols) &&
            xl6PackageValueReadStringArray(item, "requiredSymbols", &group.requiredSymbols) &&
            xl6PackageCSourceSymbols(&group.providedSymbols) &&
            xl6PackageCSourceSymbols(&group.requiredSymbols);
        for (size_t j = 0; ok && j < manifest->cSources.count; ++j)
            if (!strcmp(group.source, manifest->cSources.items[j])) ok = 0;
        for (size_t j = 0; ok && j < manifest->cSourceGroupCount; ++j) {
            const XL6_PackageCSourceGroup* prior = &manifest->cSourceGroups[j];
            if (!strcmp(prior->source, group.source)) ok = 0;
            for (size_t a = 0; ok && a < group.providedSymbols.count; ++a)
                for (size_t b = 0; b < prior->providedSymbols.count; ++b)
                    if (!strcmp(group.providedSymbols.items[a], prior->providedSymbols.items[b])) ok = 0;
        }
        XL6_PackageCSourceGroup* groups = ok ? (XL6_PackageCSourceGroup*)xrtRealloc(
            manifest->cSourceGroups, (manifest->cSourceGroupCount + 1) * sizeof(*groups)) : NULL;
        if (!groups) { xl6PackageCSourceGroupFree(&group); return 0; }
        manifest->cSourceGroups = groups;
        group.xrtModulesDeclared = xl6PackageValueHasField(artifact, "xrtModules");
        groups[manifest->cSourceGroupCount++] = group;
    }
    return 1;
}
