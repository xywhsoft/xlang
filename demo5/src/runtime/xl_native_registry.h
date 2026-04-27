#ifndef XL_NATIVE_REGISTRY_H
#define XL_NATIVE_REGISTRY_H

#include "../common/xl_common.h"

typedef struct XL_NativeModuleInfo {
	const char* sModuleKey;
	const char* sBindingName;
	const char* sVirtualPath;
	const char* sSourceText;
} XL_NativeModuleInfo;

typedef struct XL_NativeRegistryEntry {
	const char* sModuleKey;
	const char* sBindingName;
	const char* sVirtualPath;
	const char* (*fnGetSource)(void);
} XL_NativeRegistryEntry;

int xlNativeRegistryFind(const char* sModuleKey, XL_NativeModuleInfo* pInfo);

#if XLANG_DEBUGMODE_ENABLED
char* xlNativeRegistryDumpText(void);
char* xlNativeRegistryDumpJson(void);
size_t xlNativeRegistryDebugEntryCount(void);
const XL_NativeRegistryEntry* xlNativeRegistryDebugEntries(void);
#endif

#endif
