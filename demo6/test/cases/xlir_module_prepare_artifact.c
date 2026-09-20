/* Reuse exactly the native nested-Future boundary from the original six-mode
 * fixture. The generated program is this gate's private module_prepare XLIR. */
#ifdef MODULE_PREPARE_LEGACY_V2
#define xl_script_module_retirement_v3 module_prepare_owned_v3
#endif
#include "xlir_future_retirement_artifact.c"
#ifdef MODULE_PREPARE_LEGACY_V2
#undef xl_script_module_retirement_v3
/* Explicit legacy artifact boundary, not a Cgen fallback: V2 READY really
 * drains accepted work. Keep its complete original protocol population next
 * to the canonical V3 artifact which leaves graph-owned services untouched. */
static XL6_ModulePrepareResult module_prepare_legacy_drain(void)
{return xl6TaskPoolTryQuiesceV1(xlir_async_pool);}
__declspec(dllexport) const XL6_ModuleRetirementV2* xl_script_module_retirement_v2(void)
{
    static const XL6_ModuleRetirementV2 api={sizeof(api),0x584C364D525432ULL,
        &xlir_module_retirement_graph,module_prepare_legacy_drain};return &api;
}
#endif
__declspec(dllexport) XL6_TaskPool* module_prepare_pool(void) { return xlir_async_pool; }
