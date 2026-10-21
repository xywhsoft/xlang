/* 测试专用 COM 对象，不进入扩展库发布物。 */
#include "../internal/com_ole.h"
#include "com_test_fixture.h"
#include <stdlib.h>

#ifdef __TINYC__
SAFEARRAY* WINAPI SafeArrayCreateVector(VARTYPE vt, LONG lower, ULONG count);
SAFEARRAY* WINAPI SafeArrayCreate(VARTYPE vt, UINT dimensions, SAFEARRAYBOUND* bounds);
HRESULT WINAPI SafeArrayPutElement(SAFEARRAY* array, LONG* indices, void* value);
HRESULT WINAPI SafeArrayDestroy(SAFEARRAY* array);
#endif

static const IID x_com_iid_iunknown = { 0x00000000, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
static const IID x_com_iid_idispatch = { 0x00020400, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

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
#define X_COM_TEST_DISP_LONG_TEXT 6
#define X_COM_TEST_DISP_FAIL 7
#define X_COM_TEST_DISP_BIG_INT 8
#define X_COM_TEST_DISP_VALUES 9
#define X_COM_TEST_DISP_GRID 10
#define X_COM_TEST_DISP_EMPTY 11
#define X_COM_TEST_DISP_LEFT 101
#define X_COM_TEST_DISP_RIGHT 102
#define X_COM_TEST_DISP_VALUE 103

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
	if ( x_com_wide_equals_ci(name, L"LongText") ) {
		*dispid = X_COM_TEST_DISP_LONG_TEXT;
		return 1;
	}
	if ( x_com_wide_equals_ci(name, L"Fail") ) {
		*dispid = X_COM_TEST_DISP_FAIL;
		return 1;
	}
	if ( x_com_wide_equals_ci(name, L"BigInt") ) {
		*dispid = X_COM_TEST_DISP_BIG_INT;
		return 1;
	}
	if ( x_com_wide_equals_ci(name, L"Values") ) {
		*dispid = X_COM_TEST_DISP_VALUES;
		return 1;
	}
	if ( x_com_wide_equals_ci(name, L"Grid") ) {
		*dispid = X_COM_TEST_DISP_GRID;
		return 1;
	}
	if ( x_com_wide_equals_ci(name, L"Empty") ) {
		*dispid = X_COM_TEST_DISP_EMPTY;
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
	if ( params == NULL || params->rgvarg == NULL || index >= params->cArgs - params->cNamedArgs ) {
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
	if ( cNames == 1u &&
		(x_com_wide_equals_ci(rgszNames[0], L"left") ||
		x_com_wide_equals_ci(rgszNames[0], L"right") ||
		x_com_wide_equals_ci(rgszNames[0], L"value")) ) {
		return DISP_E_UNKNOWNNAME;
	}
	for ( i = 0u; i < cNames; ++i ) {
		if ( !x_com_test_name_to_dispid(rgszNames[i], &rgDispId[i]) ) {
			return DISP_E_UNKNOWNNAME;
		}
	}
	return S_OK;
}

static HRESULT x_com_test_make_values(VARIANT* result)
{
	SAFEARRAY* array = SafeArrayCreateVector(VT_VARIANT, 2, 2u);
	VARIANT item;
	LONG index = 2;
	HRESULT hr;
	if ( array == NULL ) {
		return E_OUTOFMEMORY;
	}
	VariantInit(&item);
	item.vt = VT_I4;
	item.lVal = 42;
	hr = SafeArrayPutElement(array, &index, &item);
	if ( SUCCEEDED(hr) ) {
		index = 3;
		item.vt = VT_BSTR;
		item.bstrVal = SysAllocString(L"hello");
		hr = item.bstrVal != NULL ? SafeArrayPutElement(array, &index, &item) : E_OUTOFMEMORY;
		VariantClear(&item);
	}
	if ( FAILED(hr) ) {
		SafeArrayDestroy(array);
		return hr;
	}
	result->vt = VT_ARRAY | VT_VARIANT;
	result->parray = array;
	return S_OK;
}

static HRESULT x_com_test_make_grid(VARIANT* result)
{
	SAFEARRAYBOUND bounds[2] = { { 2u, 1 }, { 2u, 3 } };
	SAFEARRAY* array = SafeArrayCreate(VT_I4, 2u, bounds);
	LONG indices[2];
	LONG value;
	if ( array == NULL ) {
		return E_OUTOFMEMORY;
	}
	for ( indices[0] = 1; indices[0] <= 2; ++indices[0] ) {
		for ( indices[1] = 3; indices[1] <= 4; ++indices[1] ) {
			value = (indices[0] - 1) * 2 + (indices[1] - 3) + 1;
			if ( FAILED(SafeArrayPutElement(array, indices, &value)) ) {
				SafeArrayDestroy(array);
				return E_INVALIDARG;
			}
		}
	}
	result->vt = VT_ARRAY | VT_I4;
	result->parray = array;
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
	(void)puArgErr;
	if ( pVarResult != NULL ) {
		VariantInit(pVarResult);
	}
	switch ( dispIdMember ) {
	case X_COM_TEST_DISP_SUB:
		left = x_com_test_arg_by_name(pDispParams, X_COM_TEST_DISP_LEFT);
		right = x_com_test_arg_by_name(pDispParams, X_COM_TEST_DISP_RIGHT);
		if ( left == NULL ) {
			left = x_com_test_arg_by_position(pDispParams, 0u);
		}
		if ( right == NULL ) {
			right = x_com_test_arg_by_position(pDispParams,
				x_com_test_arg_by_name(pDispParams, X_COM_TEST_DISP_LEFT) != NULL ? 0u : 1u);
		}
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
	case X_COM_TEST_DISP_LONG_TEXT:
		if ( pVarResult != NULL ) {
			wchar_t text[6002];
			int i;
			for ( i = 0; i < 6000; ++i ) {
				text[i] = L'A';
			}
			text[6000] = 0x4f60;
			text[6001] = 0x597d;
			pVarResult->bstrVal = SysAllocStringLen(text, 6002u);
			if ( pVarResult->bstrVal == NULL ) {
				return E_OUTOFMEMORY;
			}
			pVarResult->vt = VT_BSTR;
		}
		return S_OK;
	case X_COM_TEST_DISP_FAIL:
		if ( pExcepInfo != NULL ) {
			pExcepInfo->bstrDescription = SysAllocString(L"COM test failure");
		}
		return DISP_E_EXCEPTION;
	case X_COM_TEST_DISP_BIG_INT:
		if ( pVarResult != NULL ) {
			pVarResult->vt = VT_I8;
			pVarResult->llVal = 1099511627776LL;
		}
		return S_OK;
	case X_COM_TEST_DISP_VALUES:
		return pVarResult != NULL ? x_com_test_make_values(pVarResult) : S_OK;
	case X_COM_TEST_DISP_GRID:
		return pVarResult != NULL ? x_com_test_make_grid(pVarResult) : S_OK;
	case X_COM_TEST_DISP_EMPTY:
		if ( pVarResult != NULL ) {
			pVarResult->parray = SafeArrayCreateVector(VT_VARIANT, 0, 0u);
			if ( pVarResult->parray == NULL ) {
				return E_OUTOFMEMORY;
			}
			pVarResult->vt = VT_ARRAY | VT_VARIANT;
		}
		return S_OK;
	default:
		break;
	}
	return DISP_E_MEMBERNOTFOUND;
}

void* x_com_test_object(void)
{
	XComTestDispatch* object = (XComTestDispatch*)calloc(1, sizeof(XComTestDispatch));
	if ( object == NULL ) {
		return NULL;
	}
	object->dispatch.lpVtbl = &g_x_com_test_vtbl;
	object->refs = 1;
	object->has_child = 0;
	return &object->dispatch;
}
