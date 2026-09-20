/* Foreign and export declarations are emitted from lowered records, exactly as
 * the implementation wrappers are.  The header cannot expose an unresolved
 * source declaration: a missing C ABI is rejected by the same preflight used
 * for the generated C translation unit. */
int xl6IrCgenEmitHeader(
    XL6_Output* out, const XL6_IrModule* module, const char* guardName, XL6_DiagList* diags)
{
    size_t i;
    if ( out == NULL || guardName == NULL || guardName[0] == '\0' ||
         !xl6IrCgenVerifyEmittable(module, diags) ) return 0;
    xl6OutputPrintf(out, "#ifndef %s\n#define %s\n\n", guardName, guardName);
    xl6IrCgenEmitPublicLinkage(out);
    xl6OutputPuts("#include <stdbool.h>\n#include <stddef.h>\n#include <stdint.h>\n#include <stdlib.h>\n#include <wchar.h>\n\n"
                   "typedef struct xvalue xvalue;\n"
                   "typedef struct xlir_generator xlir_generator;\n"
                   "typedef struct xlir_closure xlir_closure;\n"
                   "typedef struct xerror xerror;\n"
                   "typedef struct xfuture xfuture;\n"
                   "typedef struct xlir_cell xlir_cell;\n"
                   "typedef struct xlir_xvalue_iterator xlir_xvalue_iterator;\n\n"
                   "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n", out);
    xl6OutputPuts(
        "#if defined(_WIN32) && (defined(XL6_USE_SHARED) || defined(XL6_BUILD_SHARED))\n"
        "/* Terminal DLL retirement: quiesce callers and release external resources first.\n"
        " * Failure forbids unload; after retirement only this retry entry is valid. */\n"
        "XLIR_C_API int xl_script_runtime_retire_checked(void);\n"
        "#endif\n\n", out);
    if (module->abi.codeOwnerAbi == XL6_IR_MODULE_CODE_OWNER_BIND_BEFORE_INIT) {
        xl6IrCgenEmitCodeOwnerDeclaration(out);
        xl6OutputPuts("/* Optional, once before any entry; host serializes binding and initialization. */\n"
            "XLIR_C_API int xl_script_bind_code_owner(const XL6_CodeOwner*);\n\n", out);
        xl6OutputPuts("/* Read-only global owning slots; caller guarantees code residency and whole-graph quiescence. */\n"
            "XLIR_C_API bool xl_script_module_ownership_trace(XL6_ModuleOwnershipVisit, void*);\n\n", out);
    }
    if ( !xl6IrCgenEmitUnionTypes(out, module, diags) ) return 0;
    if ( !xl6IrCgenEmitProductTypes(out, module, diags) ) return 0;
    if ( !xl6IrCgenEmitCallbackTypes(out, module, 0, diags) ) return 0;
    {
        size_t parameter;
        int needsLanguageFunction = 0;
        for ( i = 0; i < module->callTargetCount && !needsLanguageFunction; ++i ) {
            const XL6_IrCallTarget* target = &module->callTargets[i];
            if ( target->reachability == XL6_IR_REACHABILITY_UNREACHABLE ||
                 target->runtimeBridge != XL6_IR_RUNTIME_BRIDGE_NONE ) continue;
            for ( parameter = 0; parameter < target->parameterCount; ++parameter )
                if ( target->parameterPhysicalAbis[parameter] ==
                         XL6_IR_CALL_PARAMETER_PHYSICAL_LANGUAGE_FUNCTION_VALUE ) {
                    needsLanguageFunction = 1;
                    break;
                }
        }
        if ( needsLanguageFunction ) xl6IrCgenEmitLanguageFunctionType(out);
    }
    /* Sidecar C is compiled against this header. Use the very same physical
     * prototypes as the implementation, including native handle projection,
     * borrowed string results and raw callback typedefs. Runtime bridges own
     * their declarations and are deliberately excluded by the shared emitter. */
    for ( i = 0; i < module->callTargetCount; ++i ) {
        const XL6_IrCallTarget* target = &module->callTargets[i];
        if ( target->reachability != XL6_IR_REACHABILITY_UNREACHABLE &&
             (target->kind == XL6_IR_CALL_TARGET_EXTERN_C ||
              target->kind == XL6_IR_CALL_TARGET_NATIVE_FACTORY) &&
             !xl6IrCgenEmitExternPrototype(out, module, target, diags) ) return 0;
    }
    for ( i = 0; i < module->externGlobalCount; ++i )
        if ( !xl6IrCgenEmitExternGlobal(out, module, &module->externGlobals[i], diags) ) return 0;
    if (module->exportCount != 0) {
        xl6IrCgenEmitForeignErrorDeclarations(out);
        xl6OutputPuts(
            "extern void xrtErrorFree(xerror*);\n\n"
            "/* Export failures return zero/void and publish to the nearest shared\n"
            " * foreign error scope. A NULL diagnostic does not imply success.\n"
            " * For a total status, bracket calls with xl6ForeignCallBegin/End\n"
            " * and consume result.error with xrtErrorFree.\n"
            " * Successful entries preserve the caller's previous diagnostic. */\n\n", out);
    }
    for ( i = 0; i < module->globalExportCount; ++i ) {
        const XL6_IrGlobalExport* exported = &module->globalExports[i];
        const char* type = xl6IrCgenType(module, exported->type);
        if ( type == NULL )
            return xl6IrCgenDiag(diags, module, exported->declaration,
                                 "XLIR C header does not support global export ABI");
        xl6OutputPuts("XLIR_C_API extern ", out);
        if ( exported->mutability == XL6_IR_GLOBAL_CONST ) xl6OutputPuts("const ", out);
        xl6OutputPrintf(out, "%s %s;\n", type, exported->cName);
    }
    if ( module->globalExportCount != 0 && module->exportCount != 0 ) xl6OutputPuts("\n", out);
    for ( i = 0; i < module->exportCount; ++i ) {
        const XL6_IrExport* exported = &module->exports[i];
        const XL6_IrCallTarget* target = xl6IrCallTargetGet(module, exported->symbol);
        const char* returnType;
        size_t parameterIndex;
        if ( exported->cName == NULL || target == NULL ||
             target->kind != XL6_IR_CALL_TARGET_INTERNAL )
            return xl6IrCgenDiag(diags, module, exported->declaration,
                                 "XLIR C header export linkage ABI is unresolved");
        returnType = exported->returnAbi == XL6_IR_EXPORT_RETURN_VOID
            ? "void" : xl6IrCgenExportValueType(
                module, exported->returnType, exported->returnValueAbi);
        if ( returnType == NULL )
            return xl6IrCgenDiag(diags, module, exported->declaration,
                                 "XLIR C header does not support export return ABI");
        xl6OutputPrintf(out, "XLIR_C_API %s %s(", returnType, exported->cName);
        if ( exported->parameterCount == 0 ) xl6OutputPuts("void", out);
        for ( parameterIndex = 0; parameterIndex < exported->parameterCount; ++parameterIndex ) {
            if ( parameterIndex > 0 ) xl6OutputPuts(", ", out);
            if ( !xl6IrCgenEmitExportParameter(
                    out, module, exported, parameterIndex, 0, diags) ) return 0;
        }
        xl6OutputPuts(");\n", out);
    }
    xl6OutputPuts("\n#ifdef __cplusplus\n}\n#endif\n\n#endif\n", out);
    return !xl6OutputHasFailed(out);
}
