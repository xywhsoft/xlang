#include "src/common/xl6_xrt.h"
#include "src/compile/xl6_imports.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Exercise the same closure validator as import expansion, with real XRT
 * allocation/error/diagnostic behavior and no replacement package parser. */
#include "src/compile/xl6_import_native_features.inc"

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount);
}

static void graph(void)
{
    char* feature[] = {"GPU3D"}; char* other[] = {"gpu3d"};
    char* to_bridge[] = {"bridge"}; char* to_provider[] = {"provider"};
    char* cycle[] = {"consumer", "provider"};
    XL6_ImportPackageInfo packages[4] = {{0}};
    XL6_ImportDeps deps = {0}; deps.packageInfos = packages; deps.packageInfoCount = 4;
    packages[0].name = "consumer"; packages[1].name = "bridge";
    packages[2].name = "provider"; packages[3].name = "unrelated";
    packages[0].featureSource = "consumer.xl"; packages[0].featureLine = 7; packages[0].featureColumn = 3;
    XL6_DiagList diagnostics; xl6DiagInit(&diagnostics);
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ImportsValidateNativeFeatures(&deps, &diagnostics));
    xrtMemDebugFailClear(); balanced(&before); /* No requirements, no allocation. */
    packages[0].requiredNativeFeatures = (XL6_PackageStringList){feature, 1};
    packages[3].providedNativeFeatures = (XL6_PackageStringList){feature, 1};
    assert(!xl6ImportsValidateNativeFeatures(&deps, &diagnostics));
    assert(diagnostics.count == 1 && diagnostics.items[0].line == 7 && diagnostics.items[0].column == 3);
    assert(!strcmp(diagnostics.items[0].file, "consumer.xl"));
    assert(strstr(diagnostics.items[0].message, "GPU3D") && strstr(diagnostics.items[0].message, "consumer"));
    xl6DiagFree(&diagnostics); balanced(&before);
    packages[0].dependencies = (XL6_PackageStringList){to_bridge, 1};
    packages[0].nativeDependencyCount = 1;
    packages[1].dependencies = (XL6_PackageStringList){to_provider, 1};
    packages[1].nativeDependencyCount = 1;
    assert(!xl6ImportsValidateNativeFeatures(&deps, &diagnostics)); /* Unknown is not enabled. */
    xl6DiagFree(&diagnostics);
    packages[2].providedNativeFeatures = (XL6_PackageStringList){other, 1};
    assert(!xl6ImportsValidateNativeFeatures(&deps, &diagnostics)); /* Case-sensitive. */
    xl6DiagFree(&diagnostics);
    packages[2].providedNativeFeatures = (XL6_PackageStringList){feature, 1};
    assert(xl6ImportsValidateNativeFeatures(&deps, &diagnostics) && !diagnostics.count);
    packages[1].dependencies = (XL6_PackageStringList){cycle, 2};
    packages[1].nativeDependencyCount = 2;
    assert(xl6ImportsValidateNativeFeatures(&deps, &diagnostics)); /* Cycle, diamond and transitive provider. */
    packages[0].providedNativeFeatures = (XL6_PackageStringList){feature, 1};
    packages[0].dependencies.count = 0;
    packages[0].nativeDependencyCount = 0;
    assert(xl6ImportsValidateNativeFeatures(&deps, &diagnostics)); /* A package may provide its own native artifact. */
    for (int prefix = 0; prefix < 4; ++prefix) {
        assert(xrtMemDebugFailAfter(prefix));
        int okay = xl6ImportsValidateNativeFeatures(&deps, &diagnostics);
        xrtMemDebugFailClear();
        assert(okay == (prefix >= 2));
        xrtClearError(); xl6DiagFree(&diagnostics); balanced(&before);
    }
    xl6DiagFree(&diagnostics); balanced(&before);
    puts("Native feature closure: declared/transitive/self, unrelated rejection, cycles, case and OOM passed");
}

int main(int argc, char** argv)
{
    assert(xrtMemDebugEnable(true)); xrtClearError();
    if (argc == 1) { graph(); return 0; }
    if (!strcmp(argv[1], "--variant")) {
        assert(argc == 5);
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
        XL6_PackageManifest base, selected;
        xl6PackageManifestInit(&base); xl6PackageManifestInit(&selected);
        assert(xl6PackageLoadManifest(argv[2], &base));
        char error[512] = {0};
        int chosen = xl6PackageSelectNativeVariant(&base, XL6_NATIVE_SELECT_AUTO_AOT,
            &selected, NULL, error, sizeof(error));
        assert(chosen == atoi(argv[3]));
        if (chosen > 0) assert(selected.nativeInfo.features.count == 1 &&
                              !strcmp(selected.nativeInfo.features.items[0], argv[4]));
        else assert(strstr(error, "native variant changes"));
        xl6PackageManifestFree(&base); xl6PackageManifestFree(&selected);
        xrtClearError(); balanced(&before);
        puts("Native variant requirements preserved; selected provider features passed");
        return 0;
    }
    assert(argc >= 4);
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    XL6_PackageManifest manifest; xl6PackageManifestInit(&manifest);
    int expected = atoi(argv[2]); int loaded = xl6PackageLoadManifest(argv[1], &manifest);
    assert(loaded == expected);
    if (loaded) {
        size_t required = (size_t)atoi(argv[3]), provided = (size_t)atoi(argv[4]);
        assert(argc == (int)(5 + required + provided));
        assert(manifest.requiresNativeFeatures.count == required && manifest.nativeInfo.features.count == provided);
        for (size_t i = 0; i < required; ++i) assert(!strcmp(manifest.requiresNativeFeatures.items[i], argv[5 + i]));
        for (size_t i = 0; i < provided; ++i) assert(!strcmp(manifest.nativeInfo.features.items[i], argv[5 + required + i]));
    }
    xl6PackageManifestFree(&manifest); xrtClearError(); balanced(&before);
    if (loaded && !strcmp(argv[3], "2")) {
        int failures = 0, successes = 0;
        for (int prefix = 0; prefix < 256; ++prefix) {
            assert(xrtMemDebugFailAfter(prefix));
            xl6PackageManifestInit(&manifest); int okay = xl6PackageLoadManifest(argv[1], &manifest);
            xrtMemDebugFailClear(); failures += !okay; successes += !!okay;
            xl6PackageManifestFree(&manifest); xrtClearError(); balanced(&before);
        }
        assert(failures && successes);
    }
    puts("Native feature manifest: exact lists, rejection rollback and heap balance passed");
    return 0;
}
