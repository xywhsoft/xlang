#define INITGUID
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* demo6's embedded TCC Win32 headers do not ship OleAut/OAIdl headers. */
#ifndef XL_COM_MINIMAL_OLEAUT_DEFINED
#define XL_COM_MINIMAL_OLEAUT_DEFINED
#ifndef STDMETHODCALLTYPE
#define STDMETHODCALLTYPE WINAPI
#endif
#ifndef DISPATCH_METHOD
#define DISPATCH_METHOD 0x1
#endif
#ifndef DISPATCH_PROPERTYGET
#define DISPATCH_PROPERTYGET 0x2
#endif
#ifndef DISPATCH_PROPERTYPUT
#define DISPATCH_PROPERTYPUT 0x4
#endif
#ifndef DISPATCH_PROPERTYPUTREF
#define DISPATCH_PROPERTYPUTREF 0x8
#endif
#ifndef DISPID_PROPERTYPUT
#define DISPID_PROPERTYPUT (-3)
#endif
#ifndef COINIT_APARTMENTTHREADED
#define COINIT_APARTMENTTHREADED 0x2
#endif
#ifndef CLSCTX_INPROC_SERVER
#define CLSCTX_INPROC_SERVER 0x1
#endif
#ifndef CLSCTX_LOCAL_SERVER
#define CLSCTX_LOCAL_SERVER 0x4
#endif
#ifndef LOCALE_USER_DEFAULT
#define LOCALE_USER_DEFAULT 0x0400
#endif
#ifndef RPC_E_CHANGED_MODE
#define RPC_E_CHANGED_MODE ((HRESULT)0x80010106L)
#endif
#ifndef E_NOINTERFACE
#define E_NOINTERFACE ((HRESULT)0x80004002L)
#endif
#ifndef E_POINTER
#define E_POINTER ((HRESULT)0x80004003L)
#endif
#ifndef DISP_E_MEMBERNOTFOUND
#define DISP_E_MEMBERNOTFOUND ((HRESULT)0x80020003L)
#endif
#ifndef DISP_E_PARAMNOTFOUND
#define DISP_E_PARAMNOTFOUND ((HRESULT)0x80020004L)
#endif
#ifndef DISP_E_TYPEMISMATCH
#define DISP_E_TYPEMISMATCH ((HRESULT)0x80020005L)
#endif
#ifndef DISP_E_UNKNOWNNAME
#define DISP_E_UNKNOWNNAME ((HRESULT)0x80020006L)
#endif
#ifndef DISP_E_BADINDEX
#define DISP_E_BADINDEX ((HRESULT)0x8002000BL)
#endif
#ifndef VARIANT_TRUE
#define VARIANT_TRUE ((VARIANT_BOOL)-1)
#endif
#ifndef VARIANT_FALSE
#define VARIANT_FALSE ((VARIANT_BOOL)0)
#endif

typedef struct IDispatch IDispatch;
typedef struct IDispatchVtbl IDispatchVtbl;
typedef struct ITypeInfo ITypeInfo;
typedef struct IRecordInfo IRecordInfo;
typedef struct IUnknown IUnknown;
typedef struct IUnknownVtbl IUnknownVtbl;
typedef struct SAFEARRAY SAFEARRAY;
typedef WCHAR OLECHAR;
typedef OLECHAR* LPOLESTR;
typedef const OLECHAR* LPCOLESTR;
typedef OLECHAR* BSTR;
typedef BSTR* LPBSTR;
typedef SHORT VARIANT_BOOL;
typedef USHORT VARTYPE;
typedef double DATE;
typedef LONG DISPID;
typedef LONG SCODE;
typedef union tagCY {
	struct {
		unsigned long Lo;
		long Hi;
	};
	LONGLONG int64;
} CY;
typedef struct tagDEC {
	USHORT wReserved;
	union {
		struct {
			BYTE scale;
			BYTE sign;
		};
		USHORT signscale;
	};
	ULONG Hi32;
	union {
		struct {
			ULONG Lo32;
			ULONG Mid32;
		};
		ULONGLONG Lo64;
	};
} DECIMAL;
typedef struct tagSAFEARRAYBOUND {
	ULONG cElements;
	LONG lLbound;
} SAFEARRAYBOUND;
struct SAFEARRAY {
	USHORT cDims;
	USHORT fFeatures;
	ULONG cbElements;
	ULONG cLocks;
	PVOID pvData;
	SAFEARRAYBOUND rgsabound[1];
};
typedef struct tagVARIANT VARIANT;
struct tagVARIANT {
	union {
		struct {
			VARTYPE vt;
			WORD wReserved1;
			WORD wReserved2;
			WORD wReserved3;
			union {
				LONGLONG llVal;
				LONG lVal;
				BYTE bVal;
				SHORT iVal;
				FLOAT fltVal;
				double dblVal;
				VARIANT_BOOL boolVal;
				SCODE scode;
				CY cyVal;
				DATE date;
				BSTR bstrVal;
				IUnknown* punkVal;
				IDispatch* pdispVal;
				SAFEARRAY* parray;
				BYTE* pbVal;
				SHORT* piVal;
				LONG* plVal;
				LONGLONG* pllVal;
				FLOAT* pfltVal;
				double* pdblVal;
				VARIANT_BOOL* pboolVal;
				SCODE* pscode;
				CY* pcyVal;
				DATE* pdate;
				BSTR* pbstrVal;
				IUnknown** ppunkVal;
				IDispatch** ppdispVal;
				SAFEARRAY** pparray;
				VARIANT* pvarVal;
				PVOID byref;
				CHAR cVal;
				USHORT uiVal;
				ULONG ulVal;
				ULONGLONG ullVal;
				INT intVal;
				UINT uintVal;
				DECIMAL* pdecVal;
				CHAR* pcVal;
				USHORT* puiVal;
				ULONG* pulVal;
				ULONGLONG* pullVal;
				INT* pintVal;
				UINT* puintVal;
				struct {
					PVOID pvRecord;
					IRecordInfo* pRecInfo;
				} brecVal;
			};
		};
		DECIMAL decVal;
	};
};
typedef VARIANT VARIANTARG;

#ifndef VT_EMPTY
enum {
	VT_EMPTY = 0,
	VT_NULL = 1,
	VT_I2 = 2,
	VT_I4 = 3,
	VT_R4 = 4,
	VT_R8 = 5,
	VT_CY = 6,
	VT_DATE = 7,
	VT_BSTR = 8,
	VT_DISPATCH = 9,
	VT_ERROR = 10,
	VT_BOOL = 11,
	VT_VARIANT = 12,
	VT_UNKNOWN = 13,
	VT_DECIMAL = 14,
	VT_I1 = 16,
	VT_UI1 = 17,
	VT_UI2 = 18,
	VT_UI4 = 19,
	VT_I8 = 20,
	VT_UI8 = 21,
	VT_INT = 22,
	VT_UINT = 23,
	VT_ARRAY = 0x2000,
	VT_BYREF = 0x4000
};
#endif

typedef struct tagDISPPARAMS {
	VARIANTARG* rgvarg;
	DISPID* rgdispidNamedArgs;
	UINT cArgs;
	UINT cNamedArgs;
} DISPPARAMS;
typedef struct tagEXCEPINFO {
	WORD wCode;
	WORD wReserved;
	BSTR bstrSource;
	BSTR bstrDescription;
	BSTR bstrHelpFile;
	DWORD dwHelpContext;
	PVOID pvReserved;
	HRESULT (WINAPI* pfnDeferredFillIn)(struct tagEXCEPINFO*);
	SCODE scode;
} EXCEPINFO;

struct IUnknownVtbl {
	HRESULT (WINAPI* QueryInterface)(IUnknown* This, REFIID riid, void** ppvObject);
	ULONG (WINAPI* AddRef)(IUnknown* This);
	ULONG (WINAPI* Release)(IUnknown* This);
};
struct IUnknown {
	struct IUnknownVtbl* lpVtbl;
};

struct IDispatchVtbl {
	HRESULT (WINAPI* QueryInterface)(IDispatch* This, REFIID riid, void** ppvObject);
	ULONG (WINAPI* AddRef)(IDispatch* This);
	ULONG (WINAPI* Release)(IDispatch* This);
	HRESULT (WINAPI* GetTypeInfoCount)(IDispatch* This, UINT* pctinfo);
	HRESULT (WINAPI* GetTypeInfo)(IDispatch* This, UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo);
	HRESULT (WINAPI* GetIDsOfNames)(IDispatch* This, REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId);
	HRESULT (WINAPI* Invoke)(IDispatch* This, DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr);
};
struct IDispatch {
	struct IDispatchVtbl* lpVtbl;
};

static void x_com_VariantInit(VARIANTARG* value)
{
	if ( value != NULL ) {
		memset(value, 0, sizeof(*value));
	}
}
#define VariantInit x_com_VariantInit

HRESULT WINAPI VariantClear(VARIANTARG* pvarg);
HRESULT WINAPI VariantChangeType(VARIANTARG* pvargDest, VARIANTARG* pvarSrc, USHORT wFlags, VARTYPE vt);
BSTR WINAPI SysAllocString(const OLECHAR* text);
BSTR WINAPI SysAllocStringLen(const OLECHAR* text, UINT len);
void WINAPI SysFreeString(BSTR text);
UINT WINAPI SysStringLen(BSTR text);
UINT WINAPI SafeArrayGetDim(SAFEARRAY* array);
HRESULT WINAPI SafeArrayGetLBound(SAFEARRAY* array, UINT dim, LONG* lower);
HRESULT WINAPI SafeArrayGetUBound(SAFEARRAY* array, UINT dim, LONG* upper);
HRESULT WINAPI SafeArrayGetVartype(SAFEARRAY* array, VARTYPE* vt);
HRESULT WINAPI SafeArrayGetElement(SAFEARRAY* array, LONG* indices, void* value);
#endif

static const IID x_com_iid_null = { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };
static const IID x_com_iid_iunknown = { 0x00000000, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
static const IID x_com_iid_idispatch = { 0x00020400, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

#if defined(_WIN32) || defined(_WIN64)
#define XL_EXPORT __declspec(dllexport)
#else
#define XL_EXPORT
#endif

typedef struct XComObject {
	IDispatch* dispatch;
} XComObject;

typedef unsigned int XL_ArtifactUInt32;
typedef long long XL_ArtifactInt64;
typedef const char* XL_ArtifactStr;
typedef void* XL_ArtifactPtr;

typedef struct XL_ArtifactXValueStruct* XL_ArtifactXValue;

#define XL_ARTIFACT_XVO_DT_NULL 0
#define XL_ARTIFACT_XVO_DT_BOOL 1
#define XL_ARTIFACT_XVO_DT_INT 2
#define XL_ARTIFACT_XVO_DT_FLOAT 3
#define XL_ARTIFACT_XVO_DT_TEXT 4
#define XL_ARTIFACT_XVO_DT_POINT 6

extern XL_ArtifactXValue xvoCreateNull(void);
extern XL_ArtifactXValue xvoCreateBool(int bVal);
extern XL_ArtifactXValue xvoCreateInt(XL_ArtifactInt64 iVal);
extern XL_ArtifactXValue xvoCreateFloat(double fVal);
extern XL_ArtifactXValue xvoCreateText(void* sVal, XL_ArtifactUInt32 iSize, int bColloc);
extern XL_ArtifactXValue xvoCreatePoint(void* point);
extern XL_ArtifactXValue xvoCreateArray(void);
extern int xvoArrayAppendValue(XL_ArtifactXValue pArr, XL_ArtifactXValue pVal, int bUnrefVal);
extern XL_ArtifactUInt32 xvoArrayItemCount(XL_ArtifactXValue pArr);
extern XL_ArtifactXValue xvoArrayGetValue(XL_ArtifactXValue pArr, XL_ArtifactUInt32 iIndex);
extern int xvoType(XL_ArtifactXValue pVal);
extern int xvoGetBool(XL_ArtifactXValue pVal);
extern XL_ArtifactInt64 xvoGetInt(XL_ArtifactXValue pVal);
extern double xvoGetFloat(XL_ArtifactXValue pVal);
extern XL_ArtifactStr xvoGetText(XL_ArtifactXValue pVal);
extern XL_ArtifactPtr xvoGetPoint(XL_ArtifactXValue pVal);

static char g_x_com_text[4096];
static char g_x_com_error[512];
static int g_x_com_initialized = 0;
static int g_x_com_missing_sentinel = 0;

static XL_ArtifactXValue x_com_variant_to_xvalue(VARIANT* value);

static void x_com_copy_cstr(char* out, size_t out_size, const char* text)
{
	size_t len;

	if ( out == NULL || out_size == 0 ) {
		return;
	}
	if ( text == NULL ) {
		text = "";
	}
	len = strlen(text);
	if ( len >= out_size ) {
		len = out_size - 1;
	}
	if ( len > 0 ) {
		memcpy(out, text, len);
	}
	out[len] = '\0';
}

typedef struct XComTestDispatch {
	IDispatch dispatch;
	LONG refs;
	int has_child;
} XComTestDispatch;

static HRESULT STDMETHODCALLTYPE x_com_test_QueryInterface(IDispatch* self, REFIID riid, void** ppv);
static ULONG STDMETHODCALLTYPE x_com_test_AddRef(IDispatch* self);
static ULONG STDMETHODCALLTYPE x_com_test_Release(IDispatch* self);
static HRESULT STDMETHODCALLTYPE x_com_test_GetTypeInfoCount(IDispatch* self, UINT* pctinfo);
static HRESULT STDMETHODCALLTYPE x_com_test_GetTypeInfo(IDispatch* self, UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo);
static HRESULT STDMETHODCALLTYPE x_com_test_GetIDsOfNames(IDispatch* self, REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId);
static HRESULT STDMETHODCALLTYPE x_com_test_Invoke(IDispatch* self, DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr);

static IDispatchVtbl g_x_com_test_vtbl = {
	x_com_test_QueryInterface,
	x_com_test_AddRef,
	x_com_test_Release,
	x_com_test_GetTypeInfoCount,
	x_com_test_GetTypeInfo,
	x_com_test_GetIDsOfNames,
	x_com_test_Invoke
};

#define X_COM_TEST_DISP_SUB 1
#define X_COM_TEST_DISP_OPTIONAL_ADD 2
#define X_COM_TEST_DISP_BUMP 3
#define X_COM_TEST_DISP_CHILD 4
#define X_COM_TEST_DISP_HAS_CHILD 5
#define X_COM_TEST_DISP_LEFT 101
#define X_COM_TEST_DISP_RIGHT 102
#define X_COM_TEST_DISP_VALUE 103

static void x_com_set_error(const char* text)
{
	x_com_copy_cstr(g_x_com_error, sizeof(g_x_com_error), text);
}

static void x_com_set_hresult_error(const char* prefix, HRESULT hr)
{
	snprintf(g_x_com_error, sizeof(g_x_com_error), "%s: 0x%08lx", prefix, (unsigned long)hr);
}

static int x_com_wide_equals_ci(const wchar_t* left, const wchar_t* right)
{
	wchar_t a;
	wchar_t b;

	if ( left == NULL || right == NULL ) {
		return 0;
	}
	while ( *left != 0 && *right != 0 ) {
		a = *left++;
		b = *right++;
		if ( a >= L'A' && a <= L'Z' ) {
			a = (wchar_t)(a - L'A' + L'a');
		}
		if ( b >= L'A' && b <= L'Z' ) {
			b = (wchar_t)(b - L'A' + L'a');
		}
		if ( a != b ) {
			return 0;
		}
	}
	return (*left == 0 && *right == 0) ? 1 : 0;
}

static int x_com_test_name_to_dispid(const wchar_t* name, DISPID* dispid)
{
	if ( dispid == NULL ) {
		return 0;
	}
	if ( x_com_wide_equals_ci(name, L"Sub") ) {
		*dispid = X_COM_TEST_DISP_SUB;
		return 1;
	}
	if ( x_com_wide_equals_ci(name, L"OptionalAdd") ) {
		*dispid = X_COM_TEST_DISP_OPTIONAL_ADD;
		return 1;
	}
	if ( x_com_wide_equals_ci(name, L"Bump") ) {
		*dispid = X_COM_TEST_DISP_BUMP;
		return 1;
	}
	if ( x_com_wide_equals_ci(name, L"Child") ) {
		*dispid = X_COM_TEST_DISP_CHILD;
		return 1;
	}
	if ( x_com_wide_equals_ci(name, L"HasChild") ) {
		*dispid = X_COM_TEST_DISP_HAS_CHILD;
		return 1;
	}
	if ( x_com_wide_equals_ci(name, L"left") ) {
		*dispid = X_COM_TEST_DISP_LEFT;
		return 1;
	}
	if ( x_com_wide_equals_ci(name, L"right") ) {
		*dispid = X_COM_TEST_DISP_RIGHT;
		return 1;
	}
	if ( x_com_wide_equals_ci(name, L"value") ) {
		*dispid = X_COM_TEST_DISP_VALUE;
		return 1;
	}
	return 0;
}

static int x_com_test_variant_to_int(VARIANT* value, int fallback)
{
	VARIANT converted;
	int out = fallback;

	if ( value == NULL ) {
		return fallback;
	}
	if ( value->vt == VT_ERROR && value->scode == DISP_E_PARAMNOTFOUND ) {
		return fallback;
	}
	if ( (value->vt & VT_BYREF) != 0 && (value->vt & ~VT_BYREF) == VT_VARIANT && value->pvarVal != NULL ) {
		return x_com_test_variant_to_int(value->pvarVal, fallback);
	}
	VariantInit(&converted);
	if ( SUCCEEDED(VariantChangeType(&converted, value, 0, VT_I4)) ) {
		out = converted.lVal;
	}
	VariantClear(&converted);
	return out;
}

static VARIANT* x_com_test_arg_by_position(DISPPARAMS* params, UINT index)
{
	if ( params == NULL || params->rgvarg == NULL || index >= params->cArgs ) {
		return NULL;
	}
	return &params->rgvarg[params->cArgs - 1u - index];
}

static VARIANT* x_com_test_arg_by_name(DISPPARAMS* params, DISPID dispid)
{
	UINT i;

	if ( params == NULL || params->rgvarg == NULL || params->rgdispidNamedArgs == NULL ) {
		return NULL;
	}
	for ( i = 0u; i < params->cNamedArgs && i < params->cArgs; ++i ) {
		if ( params->rgdispidNamedArgs[i] == dispid ) {
			return &params->rgvarg[i];
		}
	}
	return NULL;
}

static VARIANT* x_com_test_arg(DISPPARAMS* params, DISPID dispid, UINT positional_index)
{
	VARIANT* value = x_com_test_arg_by_name(params, dispid);
	return (value != NULL) ? value : x_com_test_arg_by_position(params, positional_index);
}

static HRESULT STDMETHODCALLTYPE x_com_test_QueryInterface(IDispatch* self, REFIID riid, void** ppv)
{
	if ( ppv == NULL ) {
		return E_POINTER;
	}
	*ppv = NULL;
	if ( riid == NULL ) {
		return E_NOINTERFACE;
	}
	if ( IsEqualIID(riid, &x_com_iid_idispatch) || IsEqualIID(riid, &x_com_iid_iunknown) ) {
		*ppv = self;
		self->lpVtbl->AddRef(self);
		return S_OK;
	}
	return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE x_com_test_AddRef(IDispatch* self)
{
	XComTestDispatch* object = (XComTestDispatch*)self;
	return (ULONG)InterlockedIncrement(&object->refs);
}

static ULONG STDMETHODCALLTYPE x_com_test_Release(IDispatch* self)
{
	XComTestDispatch* object = (XComTestDispatch*)self;
	LONG refs = InterlockedDecrement(&object->refs);
	if ( refs == 0 ) {
		free(object);
	}
	return (ULONG)refs;
}

static HRESULT STDMETHODCALLTYPE x_com_test_GetTypeInfoCount(IDispatch* self, UINT* pctinfo)
{
	(void)self;
	if ( pctinfo != NULL ) {
		*pctinfo = 0u;
	}
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE x_com_test_GetTypeInfo(IDispatch* self, UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo)
{
	(void)self;
	(void)iTInfo;
	(void)lcid;
	if ( ppTInfo != NULL ) {
		*ppTInfo = NULL;
	}
	return DISP_E_BADINDEX;
}

static HRESULT STDMETHODCALLTYPE x_com_test_GetIDsOfNames(IDispatch* self, REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId)
{
	UINT i;

	(void)self;
	(void)riid;
	(void)lcid;
	if ( rgszNames == NULL || rgDispId == NULL ) {
		return E_POINTER;
	}
	for ( i = 0u; i < cNames; ++i ) {
		if ( !x_com_test_name_to_dispid(rgszNames[i], &rgDispId[i]) ) {
			return DISP_E_UNKNOWNNAME;
		}
	}
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE x_com_test_Invoke(IDispatch* self, DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr)
{
	XComTestDispatch* object = (XComTestDispatch*)self;
	VARIANT* left;
	VARIANT* right;
	VARIANT* value;
	int left_value;

	(void)riid;
	(void)lcid;
	(void)pExcepInfo;
	(void)puArgErr;
	if ( pVarResult != NULL ) {
		VariantInit(pVarResult);
	}
	switch ( dispIdMember ) {
	case X_COM_TEST_DISP_SUB:
		left = x_com_test_arg(pDispParams, X_COM_TEST_DISP_LEFT, 0u);
		right = x_com_test_arg(pDispParams, X_COM_TEST_DISP_RIGHT, 1u);
		if ( pVarResult != NULL ) {
			pVarResult->vt = VT_I4;
			pVarResult->lVal = x_com_test_variant_to_int(left, 0) - x_com_test_variant_to_int(right, 0);
		}
		return S_OK;
	case X_COM_TEST_DISP_OPTIONAL_ADD:
		left = x_com_test_arg(pDispParams, X_COM_TEST_DISP_LEFT, 0u);
		right = x_com_test_arg(pDispParams, X_COM_TEST_DISP_RIGHT, 1u);
		if ( pVarResult != NULL ) {
			pVarResult->vt = VT_I4;
			pVarResult->lVal = x_com_test_variant_to_int(left, 0) + x_com_test_variant_to_int(right, 10);
		}
		return S_OK;
	case X_COM_TEST_DISP_BUMP:
		value = x_com_test_arg(pDispParams, X_COM_TEST_DISP_VALUE, 0u);
		if ( value != NULL && (value->vt & VT_BYREF) != 0 && (value->vt & ~VT_BYREF) == VT_VARIANT && value->pvarVal != NULL ) {
			left_value = x_com_test_variant_to_int(value->pvarVal, 0) + 1;
			VariantClear(value->pvarVal);
			value->pvarVal->vt = VT_I4;
			value->pvarVal->lVal = left_value;
			if ( pVarResult != NULL ) {
				pVarResult->vt = VT_I4;
				pVarResult->lVal = left_value;
			}
			return S_OK;
		}
		return DISP_E_TYPEMISMATCH;
	case X_COM_TEST_DISP_CHILD:
		if ( (wFlags & DISPATCH_PROPERTYPUTREF) != 0 ) {
			object->has_child = 1;
			return S_OK;
		}
		return DISP_E_MEMBERNOTFOUND;
	case X_COM_TEST_DISP_HAS_CHILD:
		if ( pVarResult != NULL ) {
			pVarResult->vt = VT_BOOL;
			pVarResult->boolVal = object->has_child ? VARIANT_TRUE : VARIANT_FALSE;
		}
		return S_OK;
	default:
		break;
	}
	return DISP_E_MEMBERNOTFOUND;
}

static int x_com_init(void)
{
	HRESULT hr;
	if ( g_x_com_initialized ) {
		return 1;
	}
	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if ( FAILED(hr) && hr != RPC_E_CHANGED_MODE ) {
		x_com_set_hresult_error("CoInitializeEx failed", hr);
		return 0;
	}
	g_x_com_initialized = 1;
	return 1;
}

static int x_com_utf8_to_wide(const char* src, wchar_t* dst, int dst_count)
{
	int n;
	if ( dst == NULL || dst_count <= 0 ) {
		return 0;
	}
	dst[0] = 0;
	if ( src == NULL ) {
		return 0;
	}
	n = MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, dst_count);
	if ( n <= 0 ) {
		n = MultiByteToWideChar(CP_ACP, 0, src, -1, dst, dst_count);
	}
	if ( n <= 0 ) {
		dst[0] = 0;
		return 0;
	}
	dst[dst_count - 1] = 0;
	return 1;
}

static const char* x_com_wide_to_utf8(const wchar_t* src)
{
	int n;
	g_x_com_text[0] = '\0';
	if ( src == NULL ) {
		return g_x_com_text;
	}
	n = WideCharToMultiByte(CP_UTF8, 0, src, -1, g_x_com_text, (int)sizeof(g_x_com_text), NULL, NULL);
	if ( n <= 0 ) {
		WideCharToMultiByte(CP_ACP, 0, src, -1, g_x_com_text, (int)sizeof(g_x_com_text), NULL, NULL);
	}
	g_x_com_text[sizeof(g_x_com_text) - 1] = '\0';
	return g_x_com_text;
}

static int x_com_wide_to_utf8_buffer(const wchar_t* src, char* dst, int dst_count)
{
	int n;
	if ( dst == NULL || dst_count <= 0 ) {
		return 0;
	}
	dst[0] = '\0';
	if ( src == NULL ) {
		return 0;
	}
	n = WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, dst_count, NULL, NULL);
	if ( n <= 0 ) {
		n = WideCharToMultiByte(CP_ACP, 0, src, -1, dst, dst_count, NULL, NULL);
	}
	dst[dst_count - 1] = '\0';
	return n > 0;
}

static BSTR x_com_bstr_from_utf8_slice(const char* src, int src_len)
{
	wchar_t* wide;
	BSTR result;
	int wide_len;
	if ( src == NULL ) {
		return SysAllocString(L"");
	}
	if ( src_len < 0 ) {
		wide_len = MultiByteToWideChar(CP_UTF8, 0, src, -1, NULL, 0);
		if ( wide_len <= 0 ) {
			wide_len = MultiByteToWideChar(CP_ACP, 0, src, -1, NULL, 0);
			if ( wide_len <= 0 ) {
				return SysAllocString(L"");
			}
			wide = (wchar_t*)calloc((size_t)wide_len, sizeof(wchar_t));
			if ( wide == NULL ) {
				return NULL;
			}
			MultiByteToWideChar(CP_ACP, 0, src, -1, wide, wide_len);
		} else {
			wide = (wchar_t*)calloc((size_t)wide_len, sizeof(wchar_t));
			if ( wide == NULL ) {
				return NULL;
			}
			MultiByteToWideChar(CP_UTF8, 0, src, -1, wide, wide_len);
		}
		result = SysAllocString(wide);
		free(wide);
		return result;
	}
	wide_len = MultiByteToWideChar(CP_UTF8, 0, src, src_len, NULL, 0);
	if ( wide_len <= 0 ) {
		wide_len = MultiByteToWideChar(CP_ACP, 0, src, src_len, NULL, 0);
		if ( wide_len <= 0 ) {
			return SysAllocString(L"");
		}
		wide = (wchar_t*)calloc((size_t)wide_len + 1u, sizeof(wchar_t));
		if ( wide == NULL ) {
			return NULL;
		}
		MultiByteToWideChar(CP_ACP, 0, src, src_len, wide, wide_len);
	} else {
		wide = (wchar_t*)calloc((size_t)wide_len + 1u, sizeof(wchar_t));
		if ( wide == NULL ) {
			return NULL;
		}
		MultiByteToWideChar(CP_UTF8, 0, src, src_len, wide, wide_len);
	}
	result = SysAllocStringLen(wide, (UINT)wide_len);
	free(wide);
	return result;
}

static int x_com_get_dispid(IDispatch* dispatch, const char* name, DISPID* dispid)
{
	wchar_t wide_name[256];
	LPOLESTR names[1];
	HRESULT hr;
	if ( dispatch == NULL || name == NULL || dispid == NULL ) {
		x_com_set_error("invalid dispatch or member name");
		return 0;
	}
	if ( !x_com_utf8_to_wide(name, wide_name, (int)(sizeof(wide_name) / sizeof(wide_name[0]))) ) {
		x_com_set_error("member name conversion failed");
		return 0;
	}
	names[0] = wide_name;
	hr = dispatch->lpVtbl->GetIDsOfNames(dispatch, &x_com_iid_null, names, 1, LOCALE_USER_DEFAULT, dispid);
	if ( FAILED(hr) ) {
		x_com_set_hresult_error("GetIDsOfNames failed", hr);
		return 0;
	}
	return 1;
}

static int x_com_invoke0_variant(IDispatch* dispatch, const char* name, WORD flags, VARIANT* result)
{
	DISPID dispid;
	DISPPARAMS params;
	EXCEPINFO excep;
	UINT arg_error = 0;
	HRESULT hr;
	if ( result == NULL ) {
		return 0;
	}
	VariantInit(result);
	if ( !x_com_get_dispid(dispatch, name, &dispid) ) {
		return 0;
	}
	memset(&params, 0, sizeof(params));
	memset(&excep, 0, sizeof(excep));
	hr = dispatch->lpVtbl->Invoke(dispatch, dispid, &x_com_iid_null, LOCALE_USER_DEFAULT, flags, &params, result, &excep, &arg_error);
	if ( FAILED(hr) && (flags & DISPATCH_PROPERTYGET) != 0 ) {
		VariantClear(result);
		VariantInit(result);
		hr = dispatch->lpVtbl->Invoke(dispatch, dispid, &x_com_iid_null, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, result, &excep, &arg_error);
	}
	if ( FAILED(hr) ) {
		x_com_set_hresult_error("Invoke failed", hr);
		return 0;
	}
	return 1;
}

static int x_com_variant_from_xvalue(VARIANT* dst, XL_ArtifactXValue value)
{
	int type;
	XL_ArtifactInt64 int_value;
	const char* text_value;
	XL_ArtifactUInt32 text_size;
	void* point_value;
	wchar_t wide_value[1024];
	if ( dst == NULL ) {
		return 0;
	}
	VariantInit(dst);
	type = xvoType(value);
	if ( type == XL_ARTIFACT_XVO_DT_NULL ) {
		dst->vt = VT_EMPTY;
		return 1;
	}
	switch ( type ) {
	case XL_ARTIFACT_XVO_DT_BOOL:
		dst->vt = VT_BOOL;
		dst->boolVal = xvoGetBool(value) ? VARIANT_TRUE : VARIANT_FALSE;
		return 1;
	case XL_ARTIFACT_XVO_DT_INT:
		int_value = xvoGetInt(value);
		if ( int_value >= (XL_ArtifactInt64)LONG_MIN && int_value <= (XL_ArtifactInt64)LONG_MAX ) {
			dst->vt = VT_I4;
			dst->lVal = (LONG)int_value;
		} else {
			dst->vt = VT_I8;
			dst->llVal = int_value;
		}
		return 1;
	case XL_ARTIFACT_XVO_DT_FLOAT:
		dst->vt = VT_R8;
		dst->dblVal = xvoGetFloat(value);
		return 1;
	case XL_ARTIFACT_XVO_DT_TEXT:
		text_value = xvoGetText(value);
		text_size = text_value != NULL ? (XL_ArtifactUInt32)strlen(text_value) : 0u;
		dst->vt = VT_BSTR;
		dst->bstrVal = x_com_bstr_from_utf8_slice(text_value != NULL ? text_value : "", (int)text_size);
		if ( dst->bstrVal == NULL ) {
			x_com_set_error("argument allocation failed");
			return 0;
		}
		return 1;
	case XL_ARTIFACT_XVO_DT_POINT:
		point_value = xvoGetPoint(value);
		if ( point_value == (void*)&g_x_com_missing_sentinel ) {
			dst->vt = VT_ERROR;
			dst->scode = DISP_E_PARAMNOTFOUND;
			return 1;
		}
		if ( point_value != NULL ) {
			dst->vt = VT_DISPATCH;
			dst->pdispVal = (IDispatch*)point_value;
			dst->pdispVal->lpVtbl->AddRef(dst->pdispVal);
			return 1;
		}
		dst->vt = VT_EMPTY;
		return 1;
	default:
		x_com_utf8_to_wide("", wide_value, (int)(sizeof(wide_value) / sizeof(wide_value[0])));
		dst->vt = VT_BSTR;
		dst->bstrVal = SysAllocString(wide_value);
		if ( dst->bstrVal == NULL ) {
			x_com_set_error("argument allocation failed");
			return 0;
		}
		return 1;
	}
}

static int x_com_invoke_text_args_variant(IDispatch* dispatch, const char* name, int argc, const char** argv, VARIANT* result)
{
	DISPID dispid;
	DISPPARAMS params;
	EXCEPINFO excep;
	UINT arg_error = 0;
	VARIANT args[16];
	int i;
	HRESULT hr;
	if ( result == NULL ) {
		return 0;
	}
	VariantInit(result);
	if ( argc < 0 || argc > 16 ) {
		x_com_set_error("unsupported argument count");
		return 0;
	}
	if ( !x_com_get_dispid(dispatch, name, &dispid) ) {
		return 0;
	}
	for ( i = 0; i < argc; ++i ) {
		VariantInit(&args[i]);
	}
	for ( i = 0; i < argc; ++i ) {
		wchar_t wide_value[1024];
		int dst = argc - i - 1;
		const char* value = (argv != NULL && argv[i] != NULL) ? argv[i] : "";
		x_com_utf8_to_wide(value, wide_value, (int)(sizeof(wide_value) / sizeof(wide_value[0])));
		args[dst].vt = VT_BSTR;
		args[dst].bstrVal = SysAllocString(wide_value);
		if ( args[dst].bstrVal == NULL ) {
			int j;
			for ( j = 0; j < i; ++j ) {
				VariantClear(&args[argc - j - 1]);
			}
			x_com_set_error("argument allocation failed");
			return 0;
		}
	}
	memset(&params, 0, sizeof(params));
	memset(&excep, 0, sizeof(excep));
	params.rgvarg = (argc != 0) ? args : NULL;
	params.cArgs = (UINT)argc;
	hr = dispatch->lpVtbl->Invoke(dispatch, dispid, &x_com_iid_null, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, result, &excep, &arg_error);
	if ( FAILED(hr) ) {
		VariantClear(result);
		VariantInit(result);
		memset(&excep, 0, sizeof(excep));
		arg_error = 0;
		hr = dispatch->lpVtbl->Invoke(dispatch, dispid, &x_com_iid_null, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &params, result, &excep, &arg_error);
	}
	for ( i = 0; i < argc; ++i ) {
		VariantClear(&args[i]);
	}
	if ( FAILED(hr) ) {
		x_com_set_hresult_error("Invoke failed", hr);
		return 0;
	}
	return 1;
}

static int x_com_invoke_var_args_variant(IDispatch* dispatch, const char* name, int argc, XL_ArtifactXValue* argv, VARIANT* result)
{
	DISPID dispid;
	DISPPARAMS params;
	EXCEPINFO excep;
	UINT arg_error = 0;
	VARIANT args[16];
	int i;
	HRESULT hr;
	if ( result == NULL ) {
		return 0;
	}
	VariantInit(result);
	if ( argc < 0 || argc > 16 ) {
		x_com_set_error("unsupported argument count");
		return 0;
	}
	if ( !x_com_get_dispid(dispatch, name, &dispid) ) {
		return 0;
	}
	for ( i = 0; i < argc; ++i ) {
		VariantInit(&args[i]);
	}
	for ( i = 0; i < argc; ++i ) {
		int dst = argc - i - 1;
		if ( !x_com_variant_from_xvalue(&args[dst], (argv != NULL) ? argv[i] : NULL) ) {
			int j;
			for ( j = 0; j < argc; ++j ) {
				VariantClear(&args[j]);
			}
			return 0;
		}
	}
	memset(&params, 0, sizeof(params));
	memset(&excep, 0, sizeof(excep));
	params.rgvarg = (argc != 0) ? args : NULL;
	params.cArgs = (UINT)argc;
	hr = dispatch->lpVtbl->Invoke(dispatch, dispid, &x_com_iid_null, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, result, &excep, &arg_error);
	if ( FAILED(hr) ) {
		VariantClear(result);
		VariantInit(result);
		memset(&excep, 0, sizeof(excep));
		arg_error = 0;
		hr = dispatch->lpVtbl->Invoke(dispatch, dispid, &x_com_iid_null, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &params, result, &excep, &arg_error);
	}
	for ( i = 0; i < argc; ++i ) {
		VariantClear(&args[i]);
	}
	if ( FAILED(hr) ) {
		x_com_set_hresult_error("Invoke failed", hr);
		return 0;
	}
	return 1;
}

static int x_com_put_var_variant(IDispatch* dispatch, const char* name, XL_ArtifactXValue value)
{
	DISPID dispid;
	DISPID named_arg = DISPID_PROPERTYPUT;
	DISPPARAMS params;
	EXCEPINFO excep;
	UINT arg_error = 0;
	VARIANT arg;
	HRESULT hr;
	if ( dispatch == NULL ) {
		x_com_set_error("invalid COM dispatch");
		return 0;
	}
	if ( !x_com_get_dispid(dispatch, name, &dispid) ) {
		return 0;
	}
	VariantInit(&arg);
	if ( !x_com_variant_from_xvalue(&arg, value) ) {
		return 0;
	}
	memset(&params, 0, sizeof(params));
	memset(&excep, 0, sizeof(excep));
	params.rgvarg = &arg;
	params.rgdispidNamedArgs = &named_arg;
	params.cArgs = 1;
	params.cNamedArgs = 1;
	hr = dispatch->lpVtbl->Invoke(dispatch, dispid, &x_com_iid_null, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT, &params, NULL, &excep, &arg_error);
	VariantClear(&arg);
	if ( FAILED(hr) ) {
		x_com_set_hresult_error("Property put failed", hr);
		return 0;
	}
	return 1;
}

static int x_com_append_text(char* dst, size_t dst_size, size_t* used, const char* text)
{
	size_t len;
	if ( dst == NULL || used == NULL || dst_size == 0u ) {
		return 0;
	}
	if ( text == NULL ) {
		text = "";
	}
	len = strlen(text);
	if ( *used >= dst_size - 1u ) {
		return 0;
	}
	if ( len > dst_size - 1u - *used ) {
		len = dst_size - 1u - *used;
	}
	if ( len != 0u ) {
		memcpy(dst + *used, text, len);
		*used += len;
		dst[*used] = '\0';
	}
	return 1;
}

static int x_com_variant_to_utf8_buffer(VARIANT* value, char* dst, int dst_count)
{
	VARIANT text;
	HRESULT hr;
	VariantInit(&text);
	if ( dst == NULL || dst_count <= 0 ) {
		return 0;
	}
	dst[0] = '\0';
	if ( value == NULL || value->vt == VT_EMPTY || value->vt == VT_NULL ) {
		return 1;
	}
	hr = VariantChangeType(&text, value, 0, VT_BSTR);
	if ( FAILED(hr) ) {
		x_com_set_hresult_error("VariantChangeType string failed", hr);
		return 0;
	}
	x_com_wide_to_utf8_buffer(text.bstrVal, dst, dst_count);
	VariantClear(&text);
	return 1;
}

static const char* x_com_safearray_to_text(VARIANT* value)
{
	SAFEARRAY* array;
	VARTYPE item_vt;
	LONG lower;
	LONG upper;
	LONG index;
	size_t used = 0u;
	g_x_com_text[0] = '\0';
	if ( value == NULL || (value->vt & VT_ARRAY) == 0 ) {
		return g_x_com_text;
	}
	array = (value->vt & VT_BYREF) ? *value->pparray : value->parray;
	if ( array == NULL ) {
		return g_x_com_text;
	}
	if ( SafeArrayGetDim(array) != 1u ) {
		x_com_set_error("only one-dimensional SAFEARRAY is supported");
		return g_x_com_text;
	}
	if ( FAILED(SafeArrayGetLBound(array, 1u, &lower)) || FAILED(SafeArrayGetUBound(array, 1u, &upper)) ) {
		x_com_set_error("SAFEARRAY bounds failed");
		return g_x_com_text;
	}
	item_vt = value->vt & ~(VT_ARRAY | VT_BYREF);
	for ( index = lower; index <= upper; ++index ) {
		char item_text[1024];
		VARIANT item;
		HRESULT hr;
		if ( index != lower ) {
			x_com_append_text(g_x_com_text, sizeof(g_x_com_text), &used, ",");
		}
		VariantInit(&item);
		if ( item_vt == VT_VARIANT || item_vt == VT_EMPTY ) {
			hr = SafeArrayGetElement(array, &index, &item);
			if ( SUCCEEDED(hr) ) {
				x_com_variant_to_utf8_buffer(&item, item_text, (int)sizeof(item_text));
				x_com_append_text(g_x_com_text, sizeof(g_x_com_text), &used, item_text);
			}
			VariantClear(&item);
			continue;
		}
		item.vt = item_vt;
		hr = SafeArrayGetElement(array, &index, &item.byref);
		if ( SUCCEEDED(hr) ) {
			x_com_variant_to_utf8_buffer(&item, item_text, (int)sizeof(item_text));
			x_com_append_text(g_x_com_text, sizeof(g_x_com_text), &used, item_text);
		}
	}
	return g_x_com_text;
}

static int x_com_safearray_get_variant(SAFEARRAY* array, VARTYPE item_vt, LONG* indices, VARIANT* item)
{
	HRESULT hr;

	if ( (array == NULL) || (indices == NULL) || (item == NULL) ) {
		return 0;
	}
	VariantInit(item);
	if ( item_vt == VT_VARIANT || item_vt == VT_EMPTY ) {
		hr = SafeArrayGetElement(array, indices, item);
		return SUCCEEDED(hr);
	}
	item->vt = item_vt;
	switch ( item_vt ) {
	case VT_BOOL:
		hr = SafeArrayGetElement(array, indices, &item->boolVal);
		break;
	case VT_I1:
		hr = SafeArrayGetElement(array, indices, &item->cVal);
		break;
	case VT_UI1:
		hr = SafeArrayGetElement(array, indices, &item->bVal);
		break;
	case VT_I2:
		hr = SafeArrayGetElement(array, indices, &item->iVal);
		break;
	case VT_UI2:
		hr = SafeArrayGetElement(array, indices, &item->uiVal);
		break;
	case VT_I4:
	case VT_INT:
		hr = SafeArrayGetElement(array, indices, &item->lVal);
		break;
	case VT_UI4:
	case VT_UINT:
		hr = SafeArrayGetElement(array, indices, &item->ulVal);
		break;
	case VT_I8:
		hr = SafeArrayGetElement(array, indices, &item->llVal);
		break;
	case VT_UI8:
		hr = SafeArrayGetElement(array, indices, &item->ullVal);
		break;
	case VT_R4:
		hr = SafeArrayGetElement(array, indices, &item->fltVal);
		break;
	case VT_R8:
		hr = SafeArrayGetElement(array, indices, &item->dblVal);
		break;
	case VT_BSTR:
		hr = SafeArrayGetElement(array, indices, &item->bstrVal);
		break;
	case VT_DISPATCH:
		hr = SafeArrayGetElement(array, indices, &item->pdispVal);
		break;
	case VT_UNKNOWN:
		hr = SafeArrayGetElement(array, indices, &item->punkVal);
		break;
	default:
		item->vt = VT_EMPTY;
		hr = E_INVALIDARG;
		break;
	}
	return SUCCEEDED(hr);
}

static XL_ArtifactXValue x_com_safearray_to_xvalue_rec(
	SAFEARRAY* array,
	VARTYPE item_vt,
	UINT dim,
	UINT dim_count,
	LONG* lower,
	LONG* upper,
	LONG* indices
)
{
	XL_ArtifactXValue result;
	LONG index;

	result = xvoCreateArray();
	if ( result == NULL ) {
		return xvoCreateNull();
	}
	for ( index = lower[dim]; index <= upper[dim]; ++index ) {
		XL_ArtifactXValue item_value;
		indices[dim] = index;
		if ( (dim + 1u) < dim_count ) {
			item_value = x_com_safearray_to_xvalue_rec(array, item_vt, dim + 1u, dim_count, lower, upper, indices);
		} else {
			VARIANT item;
			if ( x_com_safearray_get_variant(array, item_vt, indices, &item) ) {
				item_value = x_com_variant_to_xvalue(&item);
				VariantClear(&item);
			} else {
				item_value = xvoCreateNull();
			}
		}
		if ( !xvoArrayAppendValue(result, item_value, 1) ) {
			return result;
		}
	}
	return result;
}

static XL_ArtifactXValue x_com_safearray_to_xvalue(VARIANT* value)
{
	SAFEARRAY* array;
	UINT dim_count;
	VARTYPE item_vt;
	LONG lower[8];
	LONG upper[8];
	LONG indices[8];
	UINT dim;

	if ( value == NULL || (value->vt & VT_ARRAY) == 0 ) {
		return xvoCreateNull();
	}
	array = (value->vt & VT_BYREF) ? *value->pparray : value->parray;
	if ( array == NULL ) {
		return xvoCreateArray();
	}
	dim_count = SafeArrayGetDim(array);
	if ( dim_count == 0u ) {
		return xvoCreateArray();
	}
	if ( dim_count > 8u ) {
		x_com_set_error("SAFEARRAY dimension count is too large");
		return xvoCreateNull();
	}
	for ( dim = 0u; dim < dim_count; ++dim ) {
		if ( FAILED(SafeArrayGetLBound(array, dim + 1u, &lower[dim])) ||
			FAILED(SafeArrayGetUBound(array, dim + 1u, &upper[dim])) ) {
			x_com_set_error("SAFEARRAY bounds failed");
			return xvoCreateNull();
		}
		indices[dim] = lower[dim];
	}
	item_vt = value->vt & ~(VT_ARRAY | VT_BYREF);
	return x_com_safearray_to_xvalue_rec(array, item_vt, 0u, dim_count, lower, upper, indices);
}

static const char* x_com_variant_to_text(VARIANT* value)
{
	VARIANT text;
	HRESULT hr;
	VariantInit(&text);
	if ( value == NULL || value->vt == VT_EMPTY || value->vt == VT_NULL ) {
		g_x_com_text[0] = '\0';
		return g_x_com_text;
	}
	if ( (value->vt & VT_ARRAY) != 0 ) {
		return x_com_safearray_to_text(value);
	}
	hr = VariantChangeType(&text, value, 0, VT_BSTR);
	if ( FAILED(hr) ) {
		x_com_set_hresult_error("VariantChangeType string failed", hr);
		g_x_com_text[0] = '\0';
		return g_x_com_text;
	}
	x_com_wide_to_utf8(text.bstrVal);
	VariantClear(&text);
	return g_x_com_text;
}

static XL_ArtifactXValue x_com_variant_to_xvalue(VARIANT* value)
{
	VARIANT text;
	VARTYPE vt;
	const char* array_text;
	if ( value == NULL ) {
		return xvoCreateNull();
	}
	while ( (value->vt & VT_BYREF) != 0 ) {
		vt = value->vt & ~VT_BYREF;
		if ( vt == VT_BOOL && value->pboolVal != NULL ) {
			return xvoCreateBool((*value->pboolVal) != VARIANT_FALSE);
		}
		if ( vt == VT_I4 && value->plVal != NULL ) {
			return xvoCreateInt((XL_ArtifactInt64)(*value->plVal));
		}
		if ( vt == VT_I8 && value->pllVal != NULL ) {
			return xvoCreateInt((XL_ArtifactInt64)(*value->pllVal));
		}
		if ( vt == VT_R8 && value->pdblVal != NULL ) {
			return xvoCreateFloat(*value->pdblVal);
		}
		if ( vt == VT_BSTR && value->pbstrVal != NULL ) {
			const char* sText = x_com_wide_to_utf8(*value->pbstrVal);
			return xvoCreateText((void*)sText, (XL_ArtifactUInt32)strlen(sText), 0);
		}
		return xvoCreateNull();
	}
	if ( (value->vt & VT_ARRAY) != 0 ) {
		(void)array_text;
		return x_com_safearray_to_xvalue(value);
	}
	switch ( value->vt ) {
	case VT_EMPTY:
	case VT_NULL:
		return xvoCreateNull();
	case VT_BOOL:
		return xvoCreateBool(value->boolVal != VARIANT_FALSE);
	case VT_I1:
		return xvoCreateInt((XL_ArtifactInt64)value->cVal);
	case VT_UI1:
		return xvoCreateInt((XL_ArtifactInt64)value->bVal);
	case VT_I2:
		return xvoCreateInt((XL_ArtifactInt64)value->iVal);
	case VT_UI2:
		return xvoCreateInt((XL_ArtifactInt64)value->uiVal);
	case VT_I4:
	case VT_INT:
		return xvoCreateInt((XL_ArtifactInt64)value->lVal);
	case VT_UI4:
	case VT_UINT:
		return xvoCreateInt((XL_ArtifactInt64)value->ulVal);
	case VT_I8:
		return xvoCreateInt((XL_ArtifactInt64)value->llVal);
	case VT_UI8:
		return xvoCreateInt((XL_ArtifactInt64)value->ullVal);
	case VT_R4:
		return xvoCreateFloat((double)value->fltVal);
	case VT_R8:
		return xvoCreateFloat(value->dblVal);
	case VT_BSTR:
	{
		const char* sText = x_com_wide_to_utf8(value->bstrVal);
		return xvoCreateText((void*)sText, (XL_ArtifactUInt32)strlen(sText), 0);
	}
	case VT_DISPATCH:
		if ( value->pdispVal != NULL ) {
			value->pdispVal->lpVtbl->AddRef(value->pdispVal);
		}
		return xvoCreatePoint((void*)value->pdispVal);
	case VT_UNKNOWN:
	{
		IDispatch* dispatch = NULL;
		if ( value->punkVal != NULL &&
			SUCCEEDED(value->punkVal->lpVtbl->QueryInterface(value->punkVal, &x_com_iid_idispatch, (void**)&dispatch)) ) {
			return xvoCreatePoint((void*)dispatch);
		}
		return xvoCreateNull();
	}
	default:
		VariantInit(&text);
		if ( SUCCEEDED(VariantChangeType(&text, value, 0, VT_BSTR)) ) {
			XL_ArtifactXValue result;
			const char* sText = x_com_wide_to_utf8(text.bstrVal);
			result = xvoCreateText((void*)sText, (XL_ArtifactUInt32)strlen(sText), 0);
			VariantClear(&text);
			return result;
		}
		return xvoCreateNull();
	}
}

XL_EXPORT void* x_com_create(const char* progid)
{
	wchar_t wide_progid[256];
	CLSID clsid;
	IDispatch* dispatch = NULL;
	XComObject* object;
	HRESULT hr;
	x_com_set_error("");
	if ( !x_com_init() ) {
		return NULL;
	}
	if ( !x_com_utf8_to_wide(progid, wide_progid, (int)(sizeof(wide_progid) / sizeof(wide_progid[0]))) ) {
		x_com_set_error("ProgID conversion failed");
		return NULL;
	}
	hr = CLSIDFromProgID(wide_progid, &clsid);
	if ( FAILED(hr) ) {
		x_com_set_hresult_error("CLSIDFromProgID failed", hr);
		return NULL;
	}
	hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, &x_com_iid_idispatch, (void**)&dispatch);
	if ( FAILED(hr) ) {
		x_com_set_hresult_error("CoCreateInstance failed", hr);
		return NULL;
	}
	object = (XComObject*)calloc(1, sizeof(XComObject));
	if ( object == NULL ) {
		dispatch->lpVtbl->Release(dispatch);
		x_com_set_error("out of memory");
		return NULL;
	}
	object->dispatch = dispatch;
	return object;
}

XL_EXPORT void* x_com_attach(void* dispatch)
{
	XComObject* object;
	x_com_set_error("");
	if ( dispatch == NULL ) {
		x_com_set_error("COM dispatch pointer is null");
		return NULL;
	}
	object = (XComObject*)calloc(1, sizeof(XComObject));
	if ( object == NULL ) {
		x_com_set_error("out of memory");
		return NULL;
	}
	object->dispatch = (IDispatch*)dispatch;
	return object;
}

XL_EXPORT void* x_com_test_object(void)
{
	XComTestDispatch* object = (XComTestDispatch*)calloc(1, sizeof(XComTestDispatch));
	if ( object == NULL ) {
		x_com_set_error("out of memory");
		return NULL;
	}
	object->dispatch.lpVtbl = &g_x_com_test_vtbl;
	object->refs = 1;
	object->has_child = 0;
	return &object->dispatch;
}

XL_EXPORT int x_com_release(void* handle)
{
	XComObject* object = (XComObject*)handle;
	if ( object == NULL ) {
		return 0;
	}
	if ( object->dispatch != NULL ) {
		object->dispatch->lpVtbl->Release(object->dispatch);
		object->dispatch = NULL;
	}
	free(object);
	return 1;
}

XL_EXPORT const char* x_com_last_error(void)
{
	return g_x_com_error;
}

XL_EXPORT int x_com_is_null(void* handle)
{
	return handle == NULL ? 1 : 0;
}

XL_EXPORT const char* x_com_get_text(void* handle, const char* name)
{
	XComObject* object = (XComObject*)handle;
	VARIANT result;
	const char* text;
	if ( object == NULL || object->dispatch == NULL ) {
		x_com_set_error("invalid COM handle");
		g_x_com_text[0] = '\0';
		return g_x_com_text;
	}
	if ( !x_com_invoke0_variant(object->dispatch, name, DISPATCH_PROPERTYGET, &result) ) {
		g_x_com_text[0] = '\0';
		return g_x_com_text;
	}
	text = x_com_variant_to_text(&result);
	VariantClear(&result);
	return text;
}

XL_EXPORT int x_com_get_int(void* handle, const char* name)
{
	XComObject* object = (XComObject*)handle;
	VARIANT result;
	VARIANT number;
	int value = 0;
	HRESULT hr;
	if ( object == NULL || object->dispatch == NULL ) {
		x_com_set_error("invalid COM handle");
		return 0;
	}
	if ( !x_com_invoke0_variant(object->dispatch, name, DISPATCH_PROPERTYGET, &result) ) {
		return 0;
	}
	VariantInit(&number);
	hr = VariantChangeType(&number, &result, 0, VT_I4);
	if ( SUCCEEDED(hr) ) {
		value = number.lVal;
	} else {
		x_com_set_hresult_error("VariantChangeType int failed", hr);
	}
	VariantClear(&number);
	VariantClear(&result);
	return value;
}

XL_EXPORT int x_com_put_var(void* handle, const char* name, XL_ArtifactXValue value)
{
	XComObject* object = (XComObject*)handle;
	if ( object == NULL || object->dispatch == NULL ) {
		x_com_set_error("invalid COM handle");
		return 0;
	}
	return x_com_put_var_variant(object->dispatch, name, value);
}

XL_EXPORT void* x_com_missing(void)
{
	return (void*)&g_x_com_missing_sentinel;
}

static int x_com_value_truthy(XL_ArtifactXValue value)
{
	int type = xvoType(value);
	const char* text;

	if ( type == XL_ARTIFACT_XVO_DT_NULL ) {
		return 0;
	}
	if ( type == XL_ARTIFACT_XVO_DT_BOOL ) {
		return xvoGetBool(value) != 0;
	}
	if ( type == XL_ARTIFACT_XVO_DT_INT ) {
		return xvoGetInt(value) != 0;
	}
	if ( type == XL_ARTIFACT_XVO_DT_FLOAT ) {
		return xvoGetFloat(value) != 0.0;
	}
	if ( type == XL_ARTIFACT_XVO_DT_TEXT ) {
		text = xvoGetText(value);
		return (text != NULL) && (text[0] != '\0');
	}
	return 1;
}

static const char* x_com_value_text(XL_ArtifactXValue value)
{
	const char* text;
	XL_ArtifactUInt32 size;

	if ( xvoType(value) != XL_ARTIFACT_XVO_DT_TEXT ) {
		return "";
	}
	text = xvoGetText(value);
	if ( text == NULL ) {
		return "";
	}
	size = (XL_ArtifactUInt32)strlen(text);
	if ( size >= sizeof(g_x_com_text) ) {
		memcpy(g_x_com_text, text, sizeof(g_x_com_text) - 1u);
		g_x_com_text[sizeof(g_x_com_text) - 1u] = '\0';
		return g_x_com_text;
	}
	memcpy(g_x_com_text, text, size);
	g_x_com_text[size] = '\0';
	return g_x_com_text;
}

static int x_com_named_arg_dispid(IDispatch* dispatch, XL_ArtifactXValue name_value, DISPID* dispid)
{
	const char* arg_name;

	if ( (dispatch == NULL) || (dispid == NULL) ||
		(xvoType(name_value) != XL_ARTIFACT_XVO_DT_TEXT) || (x_com_value_text(name_value)[0] == '\0') ) {
		return 0;
	}
	arg_name = x_com_value_text(name_value);
	return x_com_get_dispid(dispatch, arg_name, dispid);
}

static XL_ArtifactXValue x_com_invoke_array_internal(
	void* handle,
	const char* name,
	XL_ArtifactXValue args_value,
	XL_ArtifactXValue names_value,
	XL_ArtifactXValue byrefs_value,
	WORD flags,
	int bReturnOutArgs
)
{
	XComObject* object = (XComObject*)handle;
	DISPID dispid;
	DISPID named_ids[64];
	DISPPARAMS params;
	EXCEPINFO excep;
	UINT arg_error = 0;
	VARIANT invoke_args[64];
	VARIANT holders[64];
	VARIANT result;
	XL_ArtifactUInt32 argc;
	XL_ArtifactUInt32 i;
	UINT named_count = 0u;
	HRESULT hr;
	XL_ArtifactXValue out;

	if ( object == NULL || object->dispatch == NULL ) {
		x_com_set_error("invalid COM handle");
		return xvoCreateNull();
	}
	if ( !x_com_get_dispid(object->dispatch, name, &dispid) ) {
		return xvoCreateNull();
	}
	argc = args_value != NULL ? xvoArrayItemCount(args_value) : 0u;
	if ( argc > 64u ) {
		x_com_set_error("unsupported argument count");
		return xvoCreateNull();
	}
	memset(named_ids, 0, sizeof(named_ids));
	for ( i = 0; i < 64u; ++i ) {
		VariantInit(&invoke_args[i]);
		VariantInit(&holders[i]);
	}
	for ( i = 0; i < argc; ++i ) {
		XL_ArtifactUInt32 src = i;
		XL_ArtifactUInt32 dst = argc - 1u - i;
		XL_ArtifactXValue item = args_value != NULL ? xvoArrayGetValue(args_value, src) : NULL;
		XL_ArtifactXValue byref = byrefs_value != NULL ? xvoArrayGetValue(byrefs_value, src) : NULL;
		if ( !x_com_variant_from_xvalue(&holders[src], item) ) {
			goto fail;
		}
		if ( x_com_value_truthy(byref) ) {
			invoke_args[dst].vt = VT_VARIANT | VT_BYREF;
			invoke_args[dst].pvarVal = &holders[src];
		} else {
			invoke_args[dst] = holders[src];
			VariantInit(&holders[src]);
		}
	}
	if ( names_value != NULL ) {
		for ( i = 0; i < argc; ++i ) {
			XL_ArtifactUInt32 src = argc - 1u - i;
			XL_ArtifactXValue name_item = xvoArrayGetValue(names_value, src);
			if ( x_com_named_arg_dispid(object->dispatch, name_item, &named_ids[named_count]) ) {
				named_count += 1u;
			}
		}
	}

	memset(&params, 0, sizeof(params));
	memset(&excep, 0, sizeof(excep));
	VariantInit(&result);
	params.cArgs = argc;
	params.rgvarg = (argc != 0u) ? invoke_args : NULL;
	params.cNamedArgs = named_count;
	params.rgdispidNamedArgs = (named_count != 0u) ? named_ids : NULL;
	hr = object->dispatch->lpVtbl->Invoke(object->dispatch, dispid, &x_com_iid_null, LOCALE_USER_DEFAULT, flags, &params, &result, &excep, &arg_error);
	if ( FAILED(hr) && (flags & DISPATCH_PROPERTYGET) != 0 ) {
		VariantClear(&result);
		VariantInit(&result);
		hr = object->dispatch->lpVtbl->Invoke(object->dispatch, dispid, &x_com_iid_null, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, &result, &excep, &arg_error);
	}
	if ( FAILED(hr) ) {
		x_com_set_hresult_error("Invoke failed", hr);
		goto fail;
	}
	if ( bReturnOutArgs ) {
		out = xvoCreateArray();
		if ( out == NULL ) {
			out = xvoCreateNull();
		} else {
			xvoArrayAppendValue(out, x_com_variant_to_xvalue(&result), 1);
			for ( i = 0; i < argc; ++i ) {
				xvoArrayAppendValue(out, x_com_variant_to_xvalue(&holders[i]), 1);
			}
		}
	} else {
		out = x_com_variant_to_xvalue(&result);
	}
	VariantClear(&result);
	for ( i = 0; i < argc; ++i ) {
		VariantClear(&invoke_args[i]);
		VariantClear(&holders[i]);
	}
	return out;

fail:
	for ( i = 0; i < argc; ++i ) {
		VariantClear(&invoke_args[i]);
		VariantClear(&holders[i]);
	}
	return xvoCreateNull();
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_array(void* handle, const char* name, XL_ArtifactXValue args)
{
	return x_com_invoke_array_internal(handle, name, args, NULL, NULL, DISPATCH_METHOD | DISPATCH_PROPERTYGET, 0);
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_out(void* handle, const char* name, XL_ArtifactXValue args, XL_ArtifactXValue byrefs)
{
	return x_com_invoke_array_internal(handle, name, args, NULL, byrefs, DISPATCH_METHOD | DISPATCH_PROPERTYGET, 1);
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_named(void* handle, const char* name, XL_ArtifactXValue args, XL_ArtifactXValue names)
{
	return x_com_invoke_array_internal(handle, name, args, names, NULL, DISPATCH_METHOD | DISPATCH_PROPERTYGET, 0);
}

XL_EXPORT int x_com_put_ref(void* handle, const char* name, XL_ArtifactXValue value)
{
	XComObject* object = (XComObject*)handle;
	DISPID dispid;
	DISPID named_arg = DISPID_PROPERTYPUT;
	DISPPARAMS params;
	EXCEPINFO excep;
	UINT arg_error = 0;
	VARIANT arg;
	HRESULT hr;

	if ( object == NULL || object->dispatch == NULL ) {
		x_com_set_error("invalid COM handle");
		return 0;
	}
	if ( !x_com_get_dispid(object->dispatch, name, &dispid) ) {
		return 0;
	}
	if ( !x_com_variant_from_xvalue(&arg, value) ) {
		return 0;
	}
	memset(&params, 0, sizeof(params));
	memset(&excep, 0, sizeof(excep));
	params.cArgs = 1u;
	params.rgvarg = &arg;
	params.cNamedArgs = 1u;
	params.rgdispidNamedArgs = &named_arg;
	hr = object->dispatch->lpVtbl->Invoke(object->dispatch, dispid, &x_com_iid_null, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUTREF, &params, NULL, &excep, &arg_error);
	VariantClear(&arg);
	if ( FAILED(hr) ) {
		x_com_set_hresult_error("Property putref failed", hr);
		return 0;
	}
	return 1;
}

XL_EXPORT const char* x_com_invoke_text0(void* handle, const char* name)
{
	return x_com_get_text(handle, name);
}

static const char* x_com_invoke_text_args(void* handle, const char* name, int argc, const char** argv)
{
	XComObject* object = (XComObject*)handle;
	VARIANT result;
	const char* text;
	if ( object == NULL || object->dispatch == NULL ) {
		x_com_set_error("invalid COM handle");
		g_x_com_text[0] = '\0';
		return g_x_com_text;
	}
	if ( !x_com_invoke_text_args_variant(object->dispatch, name, argc, argv, &result) ) {
		g_x_com_text[0] = '\0';
		return g_x_com_text;
	}
	text = x_com_variant_to_text(&result);
	VariantClear(&result);
	return text;
}

static const char* x_com_invoke_var_args(void* handle, const char* name, int argc, XL_ArtifactXValue* argv)
{
	XComObject* object = (XComObject*)handle;
	VARIANT result;
	const char* text;
	if ( object == NULL || object->dispatch == NULL ) {
		x_com_set_error("invalid COM handle");
		g_x_com_text[0] = '\0';
		return g_x_com_text;
	}
	if ( !x_com_invoke_var_args_variant(object->dispatch, name, argc, argv, &result) ) {
		g_x_com_text[0] = '\0';
		return g_x_com_text;
	}
	text = x_com_variant_to_text(&result);
	VariantClear(&result);
	return text;
}

static XL_ArtifactXValue x_com_invoke_value_args(void* handle, const char* name, int argc, XL_ArtifactXValue* argv)
{
	XComObject* object = (XComObject*)handle;
	VARIANT result;
	XL_ArtifactXValue value;
	if ( object == NULL || object->dispatch == NULL ) {
		x_com_set_error("invalid COM handle");
		return xvoCreateNull();
	}
	if ( argc == 0 ) {
		if ( !x_com_invoke0_variant(object->dispatch, name, DISPATCH_PROPERTYGET, &result) ) {
			return xvoCreateNull();
		}
	} else if ( !x_com_invoke_var_args_variant(object->dispatch, name, argc, argv, &result) ) {
		return xvoCreateNull();
	}
	value = x_com_variant_to_xvalue(&result);
	VariantClear(&result);
	return value;
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_var0(void* handle, const char* name)
{
	return x_com_invoke_value_args(handle, name, 0, NULL);
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_var1(void* handle, const char* name, XL_ArtifactXValue a0)
{
	XL_ArtifactXValue args[1] = { a0 };
	return x_com_invoke_value_args(handle, name, 1, args);
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_var2(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1)
{
	XL_ArtifactXValue args[2] = { a0, a1 };
	return x_com_invoke_value_args(handle, name, 2, args);
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_var3(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2)
{
	XL_ArtifactXValue args[3] = { a0, a1, a2 };
	return x_com_invoke_value_args(handle, name, 3, args);
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_var4(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2, XL_ArtifactXValue a3)
{
	XL_ArtifactXValue args[4] = { a0, a1, a2, a3 };
	return x_com_invoke_value_args(handle, name, 4, args);
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_var5(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2, XL_ArtifactXValue a3, XL_ArtifactXValue a4)
{
	XL_ArtifactXValue args[5] = { a0, a1, a2, a3, a4 };
	return x_com_invoke_value_args(handle, name, 5, args);
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_var6(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2, XL_ArtifactXValue a3, XL_ArtifactXValue a4, XL_ArtifactXValue a5)
{
	XL_ArtifactXValue args[6] = { a0, a1, a2, a3, a4, a5 };
	return x_com_invoke_value_args(handle, name, 6, args);
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_var7(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2, XL_ArtifactXValue a3, XL_ArtifactXValue a4, XL_ArtifactXValue a5, XL_ArtifactXValue a6)
{
	XL_ArtifactXValue args[7] = { a0, a1, a2, a3, a4, a5, a6 };
	return x_com_invoke_value_args(handle, name, 7, args);
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_var8(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2, XL_ArtifactXValue a3, XL_ArtifactXValue a4, XL_ArtifactXValue a5, XL_ArtifactXValue a6, XL_ArtifactXValue a7)
{
	XL_ArtifactXValue args[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
	return x_com_invoke_value_args(handle, name, 8, args);
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_var9(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2, XL_ArtifactXValue a3, XL_ArtifactXValue a4, XL_ArtifactXValue a5, XL_ArtifactXValue a6, XL_ArtifactXValue a7, XL_ArtifactXValue a8)
{
	XL_ArtifactXValue args[9] = { a0, a1, a2, a3, a4, a5, a6, a7, a8 };
	return x_com_invoke_value_args(handle, name, 9, args);
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_var10(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2, XL_ArtifactXValue a3, XL_ArtifactXValue a4, XL_ArtifactXValue a5, XL_ArtifactXValue a6, XL_ArtifactXValue a7, XL_ArtifactXValue a8, XL_ArtifactXValue a9)
{
	XL_ArtifactXValue args[10] = { a0, a1, a2, a3, a4, a5, a6, a7, a8, a9 };
	return x_com_invoke_value_args(handle, name, 10, args);
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_var11(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2, XL_ArtifactXValue a3, XL_ArtifactXValue a4, XL_ArtifactXValue a5, XL_ArtifactXValue a6, XL_ArtifactXValue a7, XL_ArtifactXValue a8, XL_ArtifactXValue a9, XL_ArtifactXValue a10)
{
	XL_ArtifactXValue args[11] = { a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10 };
	return x_com_invoke_value_args(handle, name, 11, args);
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_var12(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2, XL_ArtifactXValue a3, XL_ArtifactXValue a4, XL_ArtifactXValue a5, XL_ArtifactXValue a6, XL_ArtifactXValue a7, XL_ArtifactXValue a8, XL_ArtifactXValue a9, XL_ArtifactXValue a10, XL_ArtifactXValue a11)
{
	XL_ArtifactXValue args[12] = { a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11 };
	return x_com_invoke_value_args(handle, name, 12, args);
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_var13(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2, XL_ArtifactXValue a3, XL_ArtifactXValue a4, XL_ArtifactXValue a5, XL_ArtifactXValue a6, XL_ArtifactXValue a7, XL_ArtifactXValue a8, XL_ArtifactXValue a9, XL_ArtifactXValue a10, XL_ArtifactXValue a11, XL_ArtifactXValue a12)
{
	XL_ArtifactXValue args[13] = { a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12 };
	return x_com_invoke_value_args(handle, name, 13, args);
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_var14(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2, XL_ArtifactXValue a3, XL_ArtifactXValue a4, XL_ArtifactXValue a5, XL_ArtifactXValue a6, XL_ArtifactXValue a7, XL_ArtifactXValue a8, XL_ArtifactXValue a9, XL_ArtifactXValue a10, XL_ArtifactXValue a11, XL_ArtifactXValue a12, XL_ArtifactXValue a13)
{
	XL_ArtifactXValue args[14] = { a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13 };
	return x_com_invoke_value_args(handle, name, 14, args);
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_var15(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2, XL_ArtifactXValue a3, XL_ArtifactXValue a4, XL_ArtifactXValue a5, XL_ArtifactXValue a6, XL_ArtifactXValue a7, XL_ArtifactXValue a8, XL_ArtifactXValue a9, XL_ArtifactXValue a10, XL_ArtifactXValue a11, XL_ArtifactXValue a12, XL_ArtifactXValue a13, XL_ArtifactXValue a14)
{
	XL_ArtifactXValue args[15] = { a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14 };
	return x_com_invoke_value_args(handle, name, 15, args);
}

XL_EXPORT XL_ArtifactXValue x_com_invoke_var16(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2, XL_ArtifactXValue a3, XL_ArtifactXValue a4, XL_ArtifactXValue a5, XL_ArtifactXValue a6, XL_ArtifactXValue a7, XL_ArtifactXValue a8, XL_ArtifactXValue a9, XL_ArtifactXValue a10, XL_ArtifactXValue a11, XL_ArtifactXValue a12, XL_ArtifactXValue a13, XL_ArtifactXValue a14, XL_ArtifactXValue a15)
{
	XL_ArtifactXValue args[16] = { a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15 };
	return x_com_invoke_value_args(handle, name, 16, args);
}

XL_EXPORT const char* x_com_invoke_text1(void* handle, const char* name, XL_ArtifactXValue a0)
{
	XL_ArtifactXValue args[1] = { a0 };
	return x_com_invoke_var_args(handle, name, 1, args);
}

XL_EXPORT const char* x_com_invoke_text2(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1)
{
	XL_ArtifactXValue args[2] = { a0, a1 };
	return x_com_invoke_var_args(handle, name, 2, args);
}

XL_EXPORT const char* x_com_invoke_text3(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2)
{
	XL_ArtifactXValue args[3] = { a0, a1, a2 };
	return x_com_invoke_var_args(handle, name, 3, args);
}

XL_EXPORT const char* x_com_invoke_text4(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2, XL_ArtifactXValue a3)
{
	XL_ArtifactXValue args[4] = { a0, a1, a2, a3 };
	return x_com_invoke_var_args(handle, name, 4, args);
}

XL_EXPORT const char* x_com_invoke_text5(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2, XL_ArtifactXValue a3, XL_ArtifactXValue a4)
{
	XL_ArtifactXValue args[5] = { a0, a1, a2, a3, a4 };
	return x_com_invoke_var_args(handle, name, 5, args);
}

XL_EXPORT const char* x_com_invoke_text6(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2, XL_ArtifactXValue a3, XL_ArtifactXValue a4, XL_ArtifactXValue a5)
{
	XL_ArtifactXValue args[6] = { a0, a1, a2, a3, a4, a5 };
	return x_com_invoke_var_args(handle, name, 6, args);
}

XL_EXPORT const char* x_com_invoke_text7(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2, XL_ArtifactXValue a3, XL_ArtifactXValue a4, XL_ArtifactXValue a5, XL_ArtifactXValue a6)
{
	XL_ArtifactXValue args[7] = { a0, a1, a2, a3, a4, a5, a6 };
	return x_com_invoke_var_args(handle, name, 7, args);
}

XL_EXPORT const char* x_com_invoke_text8(void* handle, const char* name, XL_ArtifactXValue a0, XL_ArtifactXValue a1, XL_ArtifactXValue a2, XL_ArtifactXValue a3, XL_ArtifactXValue a4, XL_ArtifactXValue a5, XL_ArtifactXValue a6, XL_ArtifactXValue a7)
{
	XL_ArtifactXValue args[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
	return x_com_invoke_var_args(handle, name, 8, args);
}
