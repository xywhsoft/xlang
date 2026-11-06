#ifndef XL_COM_OLE_H
#define XL_COM_OLE_H

#include <windows.h>
#ifndef __TINYC__
#include <oleauto.h>
#endif
#include <string.h>

/* demo6's embedded TCC Win32 headers do not ship OleAut/OAIdl headers. */
#if defined(__TINYC__) && !defined(XL_COM_MINIMAL_OLEAUT_DEFINED)
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
#ifndef DISP_E_EXCEPTION
#define DISP_E_EXCEPTION ((HRESULT)0x80020009L)
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

#endif
