/* Reuse exactly the native nested-Future boundary from the original six-mode
 * fixture. The generated program is this gate's private module_prepare XLIR. */
#define xl_script_module_retirement_v4 module_prepare_owned_v4
#include "xlir_future_retirement_artifact.c"
#undef xl_script_module_retirement_v4
#ifdef MODULE_PREPARE_LEGACY_V2
/* Explicit legacy artifact boundary, not a Cgen fallback: V2 READY really
 * drains accepted work. Keep its complete original protocol population next
 * to the canonical V4 artifact which leaves graph-owned services untouched. */
static XL6_ModulePrepareResult module_prepare_legacy_drain(void)
{return xl6TaskPoolTryQuiesceV1(xlir_async_pool);}
__declspec(dllexport) const XL6_ModuleRetirementV2* xl_script_module_retirement_v2(void)
{
    static const XL6_ModuleRetirementV2 api={sizeof(api),0x584C364D525432ULL,
        &xlir_module_retirement_graph,module_prepare_legacy_drain};return &api;
}
#else
/* Exercise the original V3 host protocol against an explicitly adapted
 * current artifact. Product Cgen continues to emit only its V4 contract. */
static XL6_ModulePrepareResult module_prepare_v3_preflight(void)
{return module_prepare_owned_v4()->preflight();}
__declspec(dllexport) const XL6_ModuleRetirementV3* xl_script_module_retirement_v3(void)
{
    static const XL6_ModuleRetirementV3 api={sizeof(api),0x584C364D525433ULL,
        &xlir_module_retirement_graph,module_prepare_v3_preflight};return &api;
}
#endif
__declspec(dllexport) XL6_TaskPool* module_prepare_pool(void) { return xlir_async_pool; }
