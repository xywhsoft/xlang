#include "xlrt_generated.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <dlfcn.h>
#endif

#define XL_SYS_LIB_KIND_KEY "__kind"
#define XL_SYS_LIB_ID_KEY "__id"
#define XL_SYS_LIB_PATH_KEY "__path"
#define XL_SYS_LIB_STATE_KEY "__state"
#define XL_SYS_LIB_HANDLE_KEY "__handle"
#define XL_SYS_LIB_KIND_VALUE "dynlib"

typedef struct XL_SysLibState {
	int64 id;
	void* handle;
	int loaded;
	int procCount;
	struct XL_SysLibState* next;
} XL_SysLibState;

static XL_SysLibState* g_xlSysLibStateHead = NULL;
static int64 g_xlSysLibNextId = 1;

static int __xlSysLibIsHandleValue(xvalue value)
{
	xvalue kindValue;
	str sKind;

	if ( !xvoIsTable(value) ) {
		return 0;
	}
	kindValue = xvoTableGetValue(value, (str)XL_SYS_LIB_KIND_KEY, (uint32)(sizeof(XL_SYS_LIB_KIND_KEY) - 1u));
	if ( !xvoIsText(kindValue) ) {
		return 0;
	}
	sKind = xvoGetText(kindValue);
	return (sKind != NULL) && (strcmp((const char*)sKind, XL_SYS_LIB_KIND_VALUE) == 0);
}

static int64 __xlSysLibId(xvalue value)
{
	xvalue idValue;

	if ( !__xlSysLibIsHandleValue(value) ) {
		return 0;
	}
	idValue = xvoTableGetValue(value, (str)XL_SYS_LIB_ID_KEY, (uint32)(sizeof(XL_SYS_LIB_ID_KEY) - 1u));
	return xvoGetInt(idValue);
}

static XL_SysLibState* __xlSysLibStateFind(int64 id)
{
	XL_SysLibState* pState;

	for ( pState = g_xlSysLibStateHead; pState != NULL; pState = pState->next ) {
		if ( pState->id == id ) {
			return pState;
		}
	}
	return NULL;
}

static XL_SysLibState* __xlSysLibStateCreate(void* pHandle)
{
	XL_SysLibState* pState;

	pState = (XL_SysLibState*)xrtMalloc(sizeof(XL_SysLibState));
	if ( pState == NULL ) {
		return NULL;
	}
	pState->id = g_xlSysLibNextId++;
	pState->handle = pHandle;
	pState->loaded = (pHandle != NULL) ? 1 : 0;
	pState->procCount = 0;
	pState->next = g_xlSysLibStateHead;
	g_xlSysLibStateHead = pState;
	return pState;
}

static XL_SysLibState* __xlSysLibStateFromValue(xvalue value)
{
	int64 id;

	id = __xlSysLibId(value);
	return __xlSysLibStateFind(id);
}

static void* __xlSysLibHandle(xvalue value)
{
	XL_SysLibState* pState;

	pState = __xlSysLibStateFromValue(value);
	if ( (pState == NULL) || !pState->loaded ) {
		return NULL;
	}
	return pState->handle;
}

static const char* __xlSysLibPath(xvalue value)
{
	xvalue pathValue;
	str sPath;

	if ( !__xlSysLibIsHandleValue(value) ) {
		return NULL;
	}
	pathValue = xvoTableGetValue(value, (str)XL_SYS_LIB_PATH_KEY, (uint32)(sizeof(XL_SYS_LIB_PATH_KEY) - 1u));
	if ( !xvoIsText(pathValue) ) {
		return NULL;
	}
	sPath = xvoGetText(pathValue);
	return (const char*)sPath;
}

static void __xlSysLibSetHandle(xvalue value, void* pHandle)
{
	XL_SysLibState* pState;
	int64 id;
	xvalue stateValue;
	xvalue handleValue;

	id = __xlSysLibId(value);
	pState = __xlSysLibStateFind(id);
	if ( pState != NULL ) {
		pState->handle = pHandle;
		pState->loaded = (pHandle != NULL) ? 1 : 0;
	}

	if ( !__xlSysLibIsHandleValue(value) ) {
		return;
	}
	stateValue = xvoTableGetValue(value, (str)XL_SYS_LIB_STATE_KEY, (uint32)(sizeof(XL_SYS_LIB_STATE_KEY) - 1u));
	if ( !xvoIsTable(stateValue) ) {
		return;
	}
	handleValue = xvoCreatePoint((ptr)pHandle);
	(void)xvoTableSetValue(
		stateValue,
		(str)XL_SYS_LIB_HANDLE_KEY,
		(uint32)(sizeof(XL_SYS_LIB_HANDLE_KEY) - 1u),
		handleValue,
		TRUE
	);
}

static xvalue __xlSysLibMakeValue(const char* sPath, void* pHandle)
{
	XL_SysLibState* pState;
	xvalue value;
	xvalue kindValue;
	xvalue idValue;
	xvalue stateValue;
	xvalue handleValue;
	xvalue pathValue;

	value = xvoCreateTable();
	if ( value == NULL ) {
		return NULL;
	}

	pState = __xlSysLibStateCreate(pHandle);
	if ( pState == NULL ) {
		xvoUnref(value);
		return NULL;
	}
	kindValue = xvoCreateText((ptr)XL_SYS_LIB_KIND_VALUE, (uint32)(sizeof(XL_SYS_LIB_KIND_VALUE) - 1u), FALSE);
	idValue = xvoCreateInt(pState->id);
	stateValue = xvoCreateTable();
	handleValue = xvoCreatePoint((ptr)pHandle);
	pathValue = xvoCreateText((ptr)((sPath != NULL) ? sPath : ""), (uint32)strlen((sPath != NULL) ? sPath : ""), FALSE);

	(void)xvoTableSetValue(value, (str)XL_SYS_LIB_KIND_KEY, (uint32)(sizeof(XL_SYS_LIB_KIND_KEY) - 1u), kindValue, TRUE);
	(void)xvoTableSetValue(value, (str)XL_SYS_LIB_ID_KEY, (uint32)(sizeof(XL_SYS_LIB_ID_KEY) - 1u), idValue, TRUE);
	(void)xvoTableSetValue(stateValue, (str)XL_SYS_LIB_HANDLE_KEY, (uint32)(sizeof(XL_SYS_LIB_HANDLE_KEY) - 1u), handleValue, TRUE);
	(void)xvoTableSetValue(value, (str)XL_SYS_LIB_STATE_KEY, (uint32)(sizeof(XL_SYS_LIB_STATE_KEY) - 1u), stateValue, TRUE);
	(void)xvoTableSetValue(value, (str)XL_SYS_LIB_PATH_KEY, (uint32)(sizeof(XL_SYS_LIB_PATH_KEY) - 1u), pathValue, TRUE);
	return value;
}

xvalue xl_sys_loadlib(const char* sPath)
{
	void* pHandle;

	if ( (sPath == NULL) || (sPath[0] == '\0') ) {
		xlrt_native_error_raise("NativeError", "sys.loadlib path is empty", 0, "");
		return xvoCreateNull();
	}

#if defined(_WIN32) || defined(_WIN64)
	pHandle = (void*)LoadLibraryA(sPath);
#else
	pHandle = dlopen(sPath, RTLD_NOW | RTLD_LOCAL);
#endif
	if ( pHandle == NULL ) {
		xlrt_native_error_raise("NativeError", "sys.loadlib failed", 0, sPath);
		return xvoCreateNull();
	}

	return __xlSysLibMakeValue(sPath, pHandle);
}

void* xl_sys_lib_proc(xvalue value, const char* sName)
{
	XL_SysLibState* pState;
	void* pHandle;
	void* pSymbol;

	if ( (sName == NULL) || (sName[0] == '\0') ) {
		xlrt_native_error_raise("NativeError", "lib.proc symbol name is empty", 0, "");
		return NULL;
	}

	pState = __xlSysLibStateFromValue(value);
	if ( (pState == NULL) || !pState->loaded || (pState->handle == NULL) ) {
		xlrt_native_error_raise("NativeError", "lib.proc expects a loadlib handle", 0, sName);
		return NULL;
	}
	pHandle = pState->handle;

#if defined(_WIN32) || defined(_WIN64)
	pSymbol = (void*)GetProcAddress((HMODULE)pHandle, sName);
#else
	pSymbol = dlsym(pHandle, sName);
#endif
	if ( pSymbol == NULL ) {
		xlrt_native_error_raise("NativeError", "lib.proc failed", 0, sName);
		return NULL;
	}

	++pState->procCount;
	return pSymbol;
}

int xl_sys_lib_close(xvalue value)
{
	XL_SysLibState* pState;
	void* pHandle;
	int bOk;

	pState = __xlSysLibStateFromValue(value);
	if ( (pState == NULL) || (pState->handle == NULL) ) {
		xlrt_native_error_raise("NativeError", "lib.close expects a loadlib handle", 0, "");
		return 0;
	}
	if ( !pState->loaded ) {
		return 0;
	}
	pHandle = pState->handle;

	if ( pState->procCount > 0 ) {
		pState->loaded = 0;
		return 1;
	}

#if defined(_WIN32) || defined(_WIN64)
	bOk = FreeLibrary((HMODULE)pHandle) ? 1 : 0;
#else
	bOk = (dlclose(pHandle) == 0) ? 1 : 0;
#endif
	if ( !bOk ) {
		xlrt_native_error_raise("NativeError", "lib.close failed", 0, "");
		return 0;
	}

	__xlSysLibSetHandle(value, NULL);
	return 1;
}

const char* xl_sys_lib_path(xvalue value)
{
	const char* sPath;

	sPath = __xlSysLibPath(value);
	if ( sPath == NULL ) {
		xlrt_native_error_raise("NativeError", "lib.path expects a loadlib handle", 0, "");
		return "";
	}

	return sPath;
}

int xl_sys_lib_loaded(xvalue value)
{
	XL_SysLibState* pState;

	pState = __xlSysLibStateFromValue(value);
	return ((pState != NULL) && pState->loaded && (pState->handle != NULL)) ? 1 : 0;
}
