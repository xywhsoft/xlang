/* Actual package parser, atomic selector and resident plan, no Native mocks. */
#include "src/common/xl6_xrt.h"
#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "include/xlc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void setup(XL6_CompileUnit* unit, const char* path)
{
    XL6_PackageManifest manifest = {0};
    assert(xl6PackageLoadManifest(path, &manifest));
    xl6CompileUnitInit(unit);
    unit->ir.stage = XL6_IR_STAGE_LOWERED;
    unit->importDeps.cSources = manifest.cSources;memset(&manifest.cSources, 0, sizeof(manifest.cSources));
    unit->importDeps.runtimeSymbols = manifest.runtimeSymbols;memset(&manifest.runtimeSymbols, 0, sizeof(manifest.runtimeSymbols));
    unit->importDeps.cSourceGroups = manifest.cSourceGroups;manifest.cSourceGroups = NULL;
    unit->importDeps.cSourceGroupCount = manifest.cSourceGroupCount;manifest.cSourceGroupCount = 0;
    xl6PackageManifestFree(&manifest);
}
static void checkPlan(const XL6_CompileUnit* unit, bool selected)
{
    XL6_ArtifactRuntimePlan plan = {0};
    assert(xl6ArtifactRuntimeRequireSymbols(&plan,
        (const char* const*)unit->importDeps.runtimeSymbols.items, unit->importDeps.runtimeSymbols.count));
    assert(xl6ArtifactRuntimeSetHas(&plan.components, XL6_ARTIFACT_NATIVE_COM) == selected);
}
static void selection(const char* path)
{
    XL6_CompileUnit unit;
    XL6_IrCallTarget target = {0};
    setup(&unit, path);
    unit.ir.callTargets = &target;unit.ir.callTargetCount = 1;
    target.cName = "coldBegin";target.kind = XL6_IR_CALL_TARGET_EXTERN_C;
    assert(xl6CompileUnitPrepareCSourceGroups(&unit));
    assert(unit.importDeps.cSources.count == 1 && unit.importDeps.runtimeSymbols.count == 1);
    assert(!unit.importDeps.cSourceGroups[0].selected);checkPlan(&unit, false);
    unit.ir.callTargets = NULL;unit.ir.callTargetCount = 0;xl6CompileUnitFree(&unit);
    for (unsigned kind = 0; kind < 3; ++kind) {
        setup(&unit, path);unit.ir.callTargets = &target;unit.ir.callTargetCount = 1;
        target.reachability = XL6_IR_REACHABILITY_TRANSITIVE;
        target.kind = kind == 0 ? XL6_IR_CALL_TARGET_EXTERN_C :
            kind == 1 ? XL6_IR_CALL_TARGET_NATIVE_FACTORY : XL6_IR_CALL_TARGET_INTERNAL;
        assert(xl6CompileUnitPrepareCSourceGroups(&unit));
        bool selected = kind != 2;
        assert(unit.importDeps.cSourceGroups[0].selected == selected);
        assert(unit.importDeps.cSources.count == (selected ? 2u : 1u));
        assert(unit.importDeps.runtimeSymbols.count == (selected ? 3u : 1u));
        checkPlan(&unit, selected);
        assert(xl6CompileUnitPrepareCSourceGroups(&unit));
        assert(unit.importDeps.cSources.count == (selected ? 2u : 1u));
        unit.ir.callTargets = NULL;unit.ir.callTargetCount = 0;xl6CompileUnitFree(&unit);
    }
    unsigned failed = 0, succeeded = 0;
    for (int limit = 0; limit < 32; ++limit) {
        setup(&unit, path);unit.ir.callTargets = &target;unit.ir.callTargetCount = 1;
        target.kind = XL6_IR_CALL_TARGET_EXTERN_C;
        assert(xrtMemDebugFailAfter(limit));
        bool ok = xl6CompileUnitPrepareCSourceGroups(&unit) != 0;
        if (!ok) assert(xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();xrtClearError();
        if (ok) { ++succeeded;assert(unit.importDeps.cSources.count == 2); }
        else {
            ++failed;assert(!unit.importDeps.cSourceGroupsResolved);
            assert(!unit.importDeps.cSourceGroups[0].selected);
            assert(unit.importDeps.cSources.count == 1 && unit.importDeps.runtimeSymbols.count == 1);
            assert(xl6CompileUnitPrepareCSourceGroups(&unit));
        }
        unit.ir.callTargets = NULL;unit.ir.callTargetCount = 0;xl6CompileUnitFree(&unit);
    }
    assert(failed && succeeded);
    printf("unreachable/direct/factory/internal/idempotence and %u OOM rollback / %u success\n", failed, succeeded);
}
int main(int argc, char** argv)
{
    if (argc == 4 && !strcmp(argv[1], "program-plan")) {
        XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit, argv[2], 1));
        assert(xl6IrLowerModule(&unit.ir, &unit.parse.diags));
        assert(xl6CompileUnitPrepareCompiledPackages(&unit));
        assert(xl6IrVerifyModule(&unit.ir, &unit.parse.diags));
        assert(xl6CompileUnitPrepareCSourceGroups(&unit));
        XL6_ArtifactRuntimePlan plan = {0};
        assert(xl6ArtifactRuntimePlanBuildWithOptions(&unit.ir,
            XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1, NULL, &plan, &unit.parse.diags));
        assert(xl6ArtifactRuntimeRequireSymbols(&plan,
            (const char* const*)unit.importDeps.runtimeSymbols.items, unit.importDeps.runtimeSymbols.count));
        bool expected = atoi(argv[3]) != 0;
        assert(xl6ArtifactRuntimeSetHas(&plan.components, XL6_ARTIFACT_NATIVE_COM) == expected);
        assert(unit.importDeps.cSourceGroups[0].selected == expected);
        xl6CompileUnitFree(&unit);return 0;
    }
    if ((argc == 7 || argc == 8) && (!strcmp(argv[1], "bridge") ||
        !strcmp(argv[1], "jit") || !strcmp(argv[1], "module"))) {
        XLC_ContextOptions options = xlcDefaultContextOptions();
        options.workDir = argv[3];options.tempDir = argv[4];
        options.tccDllPath = argv[5];options.tccLibPath = argv[6];
        XLC_Context* context = xlcCreateContext(&options);assert(context);
        assert(xlcAddIncludePath(context, argv[3]));
        if (argc == 8) assert(xlcAddIncludePath(context, argv[7]));
        if (!strcmp(argv[1], "jit")) { assert(xlcSetSaveTemps(context, 1)); }
        assert(xlcAddSourceFile(context, argv[2]));
        int ok;
        if (!strcmp(argv[1], "bridge")) ok = xlcCBridgeWithEnvironment(context, argv[4], 0);
        else if (!strcmp(argv[1], "module")) {
            ok = xlcCompile(context);
            XL_Module* module = ok ? xlcRelocateModule(context) : NULL;
            ok = module && xlRunModuleMain(xlcRuntimeContext(context), module, NULL);
            xlDestroyModule(module);
        } else ok = xlcCompile(context) && xlcRun(context, 0, NULL);
        if (!ok) fprintf(stderr, "%s\n", xlcGetLastError(context));
        xlcDestroyContext(context);return ok ? 0 : 1;
    }
    assert(argc == 4);
    if (!strcmp(argv[1], "select")) selection(argv[2]);
    else if (!strcmp(argv[1], "unknown")) {
        XL6_CompileUnit unit;setup(&unit, argv[2]);
        assert(!xl6CompileUnitPrepareCSourceGroups(&unit));
        assert(!unit.importDeps.cSourceGroupsResolved && !unit.importDeps.cSourceGroups[0].selected);
        assert(unit.importDeps.cSources.count == 1 && unit.importDeps.runtimeSymbols.count == 1);
        xl6CompileUnitFree(&unit);
    }
    else {
        XL6_PackageManifest manifest = {0};
        bool ok = xl6PackageLoadManifest(argv[2], &manifest) != 0;
        assert(ok == (atoi(argv[3]) != 0));xl6PackageManifestFree(&manifest);xrtClearError();
    }
    return 0;
}
