/* One atomic, cold selection over verified lowered call-target reachability.
 * Selected sources and resident symbols are published together and reused by
 * actual C compilation, runtime deployment and the native environment report. */
static int xl6CSourceGroupUsed(const XL6_IrModule* module,
    const XL6_PackageCSourceGroup* group)
{
    for (size_t i = 0; i < module->callTargetCount; ++i) {
        const XL6_IrCallTarget* target = &module->callTargets[i];
        if (target->reachability == XL6_IR_REACHABILITY_UNREACHABLE ||
            (target->kind != XL6_IR_CALL_TARGET_EXTERN_C &&
             target->kind != XL6_IR_CALL_TARGET_NATIVE_FACTORY) || !target->cName) continue;
        for (size_t p = 0; p < group->providedSymbols.count; ++p)
            if (!strcmp(target->cName, group->providedSymbols.items[p])) return 1;
    }
    return 0;
}
static int xl6CSourceListAppendUnique(XL6_PackageStringList* list, const char* value)
{
    for (size_t i = 0; i < list->count; ++i)
        if (!strcmp(list->items[i], value)) return 1;
    return xl6PackageStringListAppend(list, value);
}
int xl6CompileUnitPrepareCSourceGroups(XL6_CompileUnit* unit)
{
    if (!unit || unit->ir.stage != XL6_IR_STAGE_LOWERED) return 0;
    XL6_ImportDeps* deps = &unit->importDeps;
    if (deps->cSourceGroupsResolved) return 1;
    if (!deps->cSourceGroupCount) { deps->cSourceGroupsResolved = 1; return 1; }
    for (size_t i = 0; i < deps->cSourceGroupCount; ++i) {
        const XL6_PackageCSourceGroup* group = &deps->cSourceGroups[i];
        for (size_t j = 0; j < group->requiredSymbols.count; ++j)
            if (xl6ArtifactRuntimeSymbolOwner(group->requiredSymbols.items[j]) == XL6_ARTIFACT_RUNTIME_COUNT) {
                xl6DiagAdd(&unit->parse.diags, 0, 0, "C source group declares an unknown resident runtime symbol");
                return 0;
            }
    }
    XL6_PackageStringList sources = {0}, symbols = {0};
    int ok = 1, undeclared = deps->hasUndeclaredCSourceModules;
    for (size_t i = 0; ok && i < deps->cSources.count; ++i)
        ok = xl6CSourceListAppendUnique(&sources, deps->cSources.items[i]);
    for (size_t i = 0; ok && i < deps->runtimeSymbols.count; ++i)
        ok = xl6CSourceListAppendUnique(&symbols, deps->runtimeSymbols.items[i]);
    for (size_t i = 0; ok && i < deps->cSourceGroupCount; ++i) {
        const XL6_PackageCSourceGroup* group = &deps->cSourceGroups[i];
        if (!xl6CSourceGroupUsed(&unit->ir, group)) continue;
        ok = xl6CSourceListAppendUnique(&sources, group->source);
        for (size_t j = 0; ok && j < group->requiredSymbols.count; ++j)
            ok = xl6CSourceListAppendUnique(&symbols, group->requiredSymbols.items[j]);
        if (!group->xrtModulesDeclared) undeclared = 1;
    }
    if (!ok) { xl6PackageStringListFree(&sources); xl6PackageStringListFree(&symbols); return 0; }
    xl6PackageStringListFree(&deps->cSources);deps->cSources = sources;
    xl6PackageStringListFree(&deps->runtimeSymbols);deps->runtimeSymbols = symbols;
    for (size_t i = 0; i < deps->cSourceGroupCount; ++i)
        deps->cSourceGroups[i].selected = xl6CSourceGroupUsed(&unit->ir, &deps->cSourceGroups[i]);
    deps->hasUndeclaredCSourceModules = undeclared;deps->cSourceGroupsResolved = 1;
    return 1;
}
