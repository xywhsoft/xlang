/* Cold manifest validation. Only the package itself and its declared selected
 * dependency closure provide capabilities; unrelated imports cannot satisfy it.
 * This metadata does not replace native image/ABI validation at initialization. */
static int xl6ImportsValidateNativeFeatures(const XL6_ImportDeps* deps, XL6_DiagList* diags)
{
    size_t count = deps->packageInfoCount;
    int required = 0;
    for (size_t p = 0; p < count; ++p)
        required |= deps->packageInfos[p].requiredNativeFeatures.count != 0;
    if (!required) return 1; /* Legacy packages: no scratch allocation. */
    if (count > SIZE_MAX / sizeof(size_t)) return 0;
    unsigned char* seen = (unsigned char*)xrtCalloc(count, 1);
    size_t* queue = (size_t*)xrtMalloc(count * sizeof(*queue));
    if (!seen || !queue) { xrtFree(seen); xrtFree(queue); return 0; }
    int okay = 1;
    for (size_t origin = 0; okay && origin < count; ++origin) {
        const XL6_ImportPackageInfo* consumer = &deps->packageInfos[origin];
        if (!consumer->requiredNativeFeatures.count) continue;
        memset(seen, 0, count); seen[origin] = 1;
        size_t read = 0, end = 1; queue[0] = origin;
        while (read < end) {
            const XL6_PackageStringList* dependencies = &deps->packageInfos[queue[read++]].dependencies;
            for (size_t d = 0; d < dependencies->count; ++d)
                for (size_t p = 0; p < count; ++p)
                    if (!seen[p] && !strcmp(dependencies->items[d], deps->packageInfos[p].name)) {
                        seen[p] = 1; queue[end++] = p; break;
                    }
        }
        for (size_t r = 0; r < consumer->requiredNativeFeatures.count; ++r) {
            const char* feature = consumer->requiredNativeFeatures.items[r];
            int found = 0;
            for (size_t p = 0; !found && p < count; ++p) if (seen[p])
                for (size_t f = 0; f < deps->packageInfos[p].providedNativeFeatures.count; ++f)
                    if (!strcmp(feature, deps->packageInfos[p].providedNativeFeatures.items[f])) { found = 1; break; }
            if (!found) {
                char message[512];
                snprintf(message, sizeof(message), "package %s requires native feature %s; no selected provider in its declared dependency closure",
                         consumer->name, feature);
                xl6DiagAddFile(diags, consumer->featureSource,
                    consumer->featureLine > 0 ? consumer->featureLine : 1,
                    consumer->featureColumn > 0 ? consumer->featureColumn : 1, message);
                okay = 0;
            }
        }
    }
    xrtFree(queue); xrtFree(seen);
    return okay;
}
