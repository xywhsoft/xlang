#include "src/compile/xl6_xrt_trim.h"

#include "src/common/xl6_output.h"

#include <string.h>

static int xl6XrtTrimPackageRequiresFull(const XL6_ImportDeps* deps)
{
    /* A native package without a declared module contract is explicitly
     * unbounded. Runtime/compiler embedding also requests the full product. */
    return deps != NULL &&
        (deps->hasUndeclaredCSourceModules ||
         (deps->features & (XL6_IMPORT_FEATURE_TCC_RUNTIME |
                            XL6_IMPORT_FEATURE_XLANG_RUNTIME |
                            XL6_IMPORT_FEATURE_XLANG_COMPILER)) != 0);
}

static int xl6XrtTrimIsModuleName(const char* moduleName)
{
    const unsigned char* p;
    if ( moduleName == NULL || moduleName[0] == '\0' ||
         (strncmp(moduleName, "XRT_MODULE_", 11) != 0 &&
          strncmp(moduleName, "XRUNTIME_MODULE_", 16) != 0) ) {
        return 0;
    }
    for ( p = (const unsigned char*)moduleName; *p != '\0'; ++p ) {
        if ( !((*p >= 'A' && *p <= 'Z') ||
               (*p >= '0' && *p <= '9') || *p == '_') ) return 0;
    }
    return 1;
}

static int xl6XrtTrimEmitModule(XL6_Output* out, const char* moduleName)
{
    return xl6XrtTrimIsModuleName(moduleName) &&
           xl6OutputPrintf(out, "#define %s\n", moduleName) != EOF;
}

static int xl6XrtTrimEmitModules(
    XL6_Output* out,
    const char* const* modules,
    size_t moduleCount)
{
    size_t i;
    if ( moduleCount != 0 && modules == NULL ) return 0;
    for ( i = 0; i < moduleCount; ++i ) {
        size_t previous;
        if ( !xl6XrtTrimIsModuleName(modules[i]) ) return 0;
        for ( previous = 0; previous < i; ++previous ) {
            if ( strcmp(modules[previous], modules[i]) == 0 ) break;
        }
        if ( previous == i && !xl6XrtTrimEmitModule(out, modules[i]) ) return 0;
    }
    return 1;
}

int xl6XrtTrimBuildImplementationSourceFromArtifact(
    const XL6_ImportDeps* deps,
    const char* const* modules,
    size_t moduleCount,
    char** outText)
{
    XL6_Output out;
    int full;
    int ok;

    if ( outText == NULL || (moduleCount != 0 && modules == NULL) ) return 0;
    *outText = NULL;
    full = xl6XrtTrimPackageRequiresFull(deps);
    if ( !xl6OutputOpenMemory(&out) ) return 0;
    ok = xl6OutputPuts(
             "/* Generated XRT implementation for a verified xlang artifact. */\n",
             &out) != EOF;
    if ( ok && full ) {
        ok = xl6XrtTrimEmitModule(&out, "XRT_MODULE_ALL") &&
             xl6XrtTrimEmitModule(&out, "XRUNTIME_MODULE_ALL");
    }
    if ( ok && !full && deps != NULL ) {
        ok = xl6XrtTrimEmitModules(
            &out, (const char* const*)deps->xrtModules.items,
            deps->xrtModules.count);
    }
    if ( ok && !full ) ok = xl6XrtTrimEmitModules(&out, modules, moduleCount);
    if ( ok ) {
        ok = xl6OutputPuts(
                 "#define XRUNTIME_IMPLEMENTATION\n"
                 "#include \"xruntime.h\"\n",
                 &out) != EOF && !xl6OutputHasFailed(&out);
    }
    if ( ok ) {
        *outText = xl6OutputTakeMemoryText(&out, NULL);
        ok = *outText != NULL;
    }
    xl6OutputClose(&out);
    return ok;
}
