#include "xl_native_registry.h"

#if XLANG_DEBUGMODE_ENABLED

const char* xlStdDebugModuleSource(void);
const char* xlStdTraceModuleSource(void);

static const XL_NativeRegistryEntry g_arrDebugNativeModules[] = {
	{ "std.debug", "std.debug", "<native:std.debug>", xlStdDebugModuleSource },
	{ "std.trace", "std.trace", "<native:std.trace>", xlStdTraceModuleSource }
};

size_t xlNativeRegistryDebugEntryCount(void)
{
	return XL_COUNTOF(g_arrDebugNativeModules);
}

const XL_NativeRegistryEntry* xlNativeRegistryDebugEntries(void)
{
	return g_arrDebugNativeModules;
}

#endif
