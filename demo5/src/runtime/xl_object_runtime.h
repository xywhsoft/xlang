#ifndef XL_OBJECT_RUNTIME_H
#define XL_OBJECT_RUNTIME_H

#include "../common/xl_common.h"
#include "../../lib/xrt/xrt.h"

#define XL_OBJECT_HEADER_MAGIC 0x584C4F42u
#define XL_OBJECT_FLAG_DESTROYING 0x00000001u
#define XL_OBJECT_FLAG_DESTROYED  0x00000002u

#ifndef XL_OBJECT_TYPE_INFO_DEFINED
#define XL_OBJECT_TYPE_INFO_DEFINED
typedef struct XL_ObjectMethodEntry {
	const char* sDispatchKey;
	void* pFunc;
} XL_ObjectMethodEntry;

typedef struct XL_ObjectWeakRef XL_ObjectWeakRef;

typedef struct XL_ObjectTypeInfo {
	const char* sName;
	const struct XL_ObjectTypeInfo* pBaseType;
	void (*pDestroyFunc)(void* pBlob);
	const XL_ObjectMethodEntry* pMethods;
	size_t iMethodCount;
} XL_ObjectTypeInfo;
#endif

#ifndef XL_OBJECT_HEADER_DEFINED
#define XL_OBJECT_HEADER_DEFINED
typedef struct XL_ObjectHeader {
	uint32_t uMagic;
	uint32_t uFlags;
	const XL_ObjectTypeInfo* pTypeInfo;
	void* pNativePayload;
	xvalue dynamicFields;
	XL_ObjectWeakRef* pWeakRefs;
	xvalue pOwner;
	struct XL_ObjectHeader* pTrackPrev;
	struct XL_ObjectHeader* pTrackNext;
} XL_ObjectHeader;
#endif

struct XL_ObjectWeakRef {
	XL_ObjectWeakRef* pPrev;
	XL_ObjectWeakRef* pNext;
	xvalue pValue;
	void* pBlob;
};

int xl_object_header_ready(const void* pBlob);
int xl_object_header_alive(const void* pBlob);
int xl_object_header_destroying(const void* pBlob);
int xl_object_header_destroyed(const void* pBlob);
void xl_object_header_init(void* pBlob);
void xl_object_header_bind(void* pBlob, const XL_ObjectTypeInfo* pTypeInfo);
void xl_object_header_clone(void* pDstBlob, const void* pSrcBlob, const XL_ObjectTypeInfo* pFallbackTypeInfo);
void xl_object_header_dispose(void* pBlob);
const XL_ObjectTypeInfo* xl_object_type(const void* pBlob);
void* xl_object_payload(const void* pBlob);
int xl_object_payload_set(void* pBlob, void* pPayload);
void* xl_object_payload_take(void* pBlob);
void* xl_object_dispatch(void* pBlob, const char* sDispatchKey);
int xl_object_delete_value(xvalue value);
void xl_object_destroy_if_last(xvalue value);
xvalue xl_object_bind_value(xvalue value);
xvalue xl_object_retain(xvalue value);
uint32_t xl_object_refcount(xvalue value);
void xl_object_release(xvalue value);
XL_ObjectWeakRef* xl_object_weak_new(xvalue value);
void xl_object_weak_delete(XL_ObjectWeakRef* pWeakRef);
int xl_object_weak_alive(const XL_ObjectWeakRef* pWeakRef);
xvalue xl_object_weak_lock(XL_ObjectWeakRef* pWeakRef);
xvalue xl_object_dynamic_fields(void* pBlob);
int xl_object_length(void* pBlob, int iDeclaredFieldCount);
int xl_object_exists(void* pBlob, const char* sKey);
xvalue xl_object_get(void* pBlob, const char* sKey, xvalue defaultValue, int bHasDefault);
int xl_object_set(void* pBlob, const char* sKey, xvalue value);
int xl_object_remove(void* pBlob, const char* sKey);
void xl_object_clear(void* pBlob);
xvalue xl_object_keys(void* pBlob);
xvalue xl_object_values(void* pBlob);
xvalue xl_object_items(void* pBlob);
int xl_object_collect_cycles(void);

#endif
