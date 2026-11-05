/* Native symbols and relocations stay authoritative in the actual archive.
 * The manifest declares only opt-in compilation/link requirements. */
static int xl6PackageValueReadNativeLink(const xvalue* artifact,
    XL6_PackageManifest* manifest)
{
    static const char* const names[] = {
        "functionSections", "dataSections", "gcSections", "printGcSections"
    };
    static const unsigned int flags[] = {
        XL6_NATIVE_LINK_FUNCTION_SECTIONS, XL6_NATIVE_LINK_DATA_SECTIONS,
        XL6_NATIVE_LINK_GC_SECTIONS, XL6_NATIVE_LINK_PRINT_GC_SECTIONS
    };
    if (!xl6PackageValueHasField(artifact, "nativeLink")) return 1;
    const xvalue* config = xl6PackageValueField(artifact, "nativeLink");
    if (xrtValueType(config) != XVALUE_OBJECT) return 0;
    unsigned int result = 0;
    for (size_t i = 0; i < xrtValueCount(config); ++i) {
        xstrview name = {0};
        const xvalue* value = xrtValueObjectAt(config, i, &name);
        size_t field = 0;
        for (; field < sizeof(names) / sizeof(names[0]); ++field)
            if (name.Size == strlen(names[field]) &&
                !memcmp(name.Data, names[field], name.Size)) break;
        bool enabled = false;
        if (field == sizeof(names) / sizeof(names[0]) ||
            xrtValueType(value) != XVALUE_BOOL || !xrtValueGetBool(value, &enabled)) return 0;
        if (enabled) result |= flags[field];
    }
    if ((result & XL6_NATIVE_LINK_PRINT_GC_SECTIONS) &&
        !(result & XL6_NATIVE_LINK_GC_SECTIONS)) return 0;
    manifest->nativeLinkFlags = result;
    return 1;
}
