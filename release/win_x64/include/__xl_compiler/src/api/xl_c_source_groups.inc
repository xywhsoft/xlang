static xvalue* xlCSourceGroupStrings(const XL6_PackageStringList* list)
{
    xvalue* array = xrtValueArray();
    if (!array) return NULL;
    for (size_t i = 0; i < list->count; ++i) {
        xvalue* text = xlCEnvString(list->items[i]);
        int ok = text && xrtValueArrayAppend(array, text);
        xrtValueRelease(text);
        if (!ok) { xrtValueRelease(array); return NULL; }
    }
    return array;
}
static int xlCSourceGroupReport(xvalue* root, const XL6_CompileUnit* unit)
{
    if (!unit) return 1;
    const XL6_ImportDeps* deps = &unit->importDeps;
    xvalue* array = xrtValueArray();
    int ok = array != NULL;
    for (size_t i = 0; ok && i < deps->cSourceGroupCount; ++i) {
        const XL6_PackageCSourceGroup* group = &deps->cSourceGroups[i];
        xvalue* row = xrtValueObject();
        ok = row && xlCEnvPut(row, "source", xlCEnvString(group->source)) &&
            xlCEnvPut(row, "providedSymbols", xlCSourceGroupStrings(&group->providedSymbols)) &&
            xlCEnvPut(row, "requiredSymbols", xlCSourceGroupStrings(&group->requiredSymbols)) &&
            xlCEnvPut(row, "selected", xrtValueBool(group->selected != 0)) &&
            xrtValueArrayAppend(array, row);
        xrtValueRelease(row);
    }
    if (ok) ok = xrtValueObjectSet(root, xrtStrView("cSourceGroups"), array) &&
        xlCEnvPut(root, "selectedCSources", xlCSourceGroupStrings(&deps->cSources)) &&
        xlCEnvPut(root, "requiredRuntimeSymbols", xlCSourceGroupStrings(&deps->runtimeSymbols));
    xrtValueRelease(array);
    return ok;
}
