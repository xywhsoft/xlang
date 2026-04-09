#ifndef XL_NATIVE_REGISTRY_H
#define XL_NATIVE_REGISTRY_H

#include "../common/xl_common.h"

typedef struct XL_NativeModuleInfo {
	const char* sModuleKey;
	const char* sBindingName;
	const char* sVirtualPath;
	const char* sSourceText;
} XL_NativeModuleInfo;

int xlNativeRegistryFind(const char* sModuleKey, XL_NativeModuleInfo* pInfo);
char* xlNativeRegistryDumpText(void);
char* xlNativeRegistryDumpJson(void);

#endif
