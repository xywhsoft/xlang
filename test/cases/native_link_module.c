/* Actual public compiler/API integration for opt-in native section linking.
 * All providers, packages and SDK paths are explicit process arguments.
 * The language body and Native archive are compiled by the selected TinyCC;
 * this host never substitutes a C implementation of the script. */
#include "xlc.h"
#include "src/api/xl_module_state.h"
#include "src/api/xl_foreign_error.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned int dropped;
static void native_link_record_drop(int id)
{
    if (id == 71) ++dropped;
}

static int failed(XLC_Context* compiler, const char* phase)
{
    fprintf(stderr, "%s: %s\n", phase,
        xlcGetLastError(compiler) ? xlcGetLastError(compiler) : "no diagnostic");
    return 10;
}

int main(int argc, char** argv)
{
    /* source workdir sdk provider libraries temporary mode [external-output] */
    if (argc < 8) return 2;
    XLC_ContextOptions options = xlcDefaultContextOptions();
    options.workDir = argv[2]; options.tccDllPath = argv[4];
    options.tccLibPath = argv[5]; options.tempDir = argv[6];
    XLC_Context* compiler = xlcCreateContext(&options);
    if (!compiler) return 3;
    if (argc > 8 && strcmp(argv[7], "exe") &&
        !xlcAddIncludePath(compiler, argv[8])) return 5;
    if (!xlcAddIncludePath(compiler, argv[3]) ||
        !xlcRegisterCFunction(compiler, "native_link_record_drop", (void*)native_link_record_drop) ||
        !xlcAddSourceFile(compiler, argv[1]) || !xlcCompile(compiler))
        return failed(compiler, "compile");
    if (!strcmp(argv[7], "reject")) {
        XL_Module* module = xlcRelocateModule(compiler);
        const char* error = xlcGetLastError(compiler);
        if (module || !error || !strstr(error, "nativeLink requirements"))
            return failed(compiler, "expected capability rejection");
        puts(error); xlcDestroyContext(compiler); return 0;
    }
    if (!strcmp(argv[7], "exe")) {
        if (argc != 9 || !xlcEmitExe(compiler, argv[8]))
            return failed(compiler, "emit executable");
        xlcDestroyContext(compiler); return 0;
    }
    if (strcmp(argv[7], "module")) return 4;
    XL_Module* module = xlcRelocateModule(compiler);
    if (!module) return failed(compiler, "relocate");
    typedef int (*CheckedMain)(xvalue*, int*);
    typedef int64_t (*Export)(int64_t);
    CheckedMain entry = (CheckedMain)xlModuleGetSymbol(module, "xl_script_main_checked");
    Export exported = (Export)xlModuleGetSymbol(module, "native_link_unreferenced_export");
    Export raises = (Export)xlModuleGetSymbol(module, "native_link_checked_error");
    /* These are looked up by the real host, rather than referenced from C.
     * Keeping only the language call graph would incorrectly discard them. */
    if (!entry || !exported || !raises ||
        !module->ownershipTrace || !module->ownershipReadContract ||
        !module->retirementContract || !module->retirementPreparation ||
        !module->codeOwnerReady ||
        !xlModuleGetSymbol(module, "xl_script_module_unit_checked") ||
        !xlModuleGetSymbol(module, "xl_script_bind_code_owner") ||
        xlModuleGetSymbol(module, "native_link_dead") ||
        xlModuleGetSymbol(module, "native_link_dead_data")) return 11;
    XL_Context* context = xlcRuntimeContext(compiler);
    if (!xlActivateModule(context, module, NULL)) return failed(compiler, "activate");
    int code = -1;
    if (!entry(NULL, &code) || code || dropped) return 12;
    xlcDestroyContext(compiler); compiler = NULL;
    /* The live global class legitimately owns its finalizer code lease. */
    size_t baselineLeases = module->codeLeases;
    xl6_foreign_error_scope scope = {0};
    if (!xl6ForeignCallBegin(&scope)) return 13;
    int64_t result = exported(5);
    xl6_foreign_error_result checked = xl6ForeignCallEnd(&scope);
    if (!checked.ok || checked.error || result != 47 || dropped) return 14;
    if (!xl6ForeignCallBegin(&scope)) return 15;
    (void)raises(0);
    checked = xl6ForeignCallEnd(&scope);
    if (checked.ok || !checked.error) return 16;
    xrtErrorFree(checked.error);
    if (module->activeCalls || module->codeLeases != baselineLeases) return 17;
    xlDestroyModule(module);
    XL_RuntimeDrainResult drained = xlRuntimeDrainRetiringChecked(-1);
    if (!drained.ready || !drained.ok || dropped != 1 || xrtGetError()) return 18;
    puts("nativeLink: dead unresolved section discarded; unused export and checked ABI survived; compiler-independent module retired its global exactly once");
    return 0;
}
