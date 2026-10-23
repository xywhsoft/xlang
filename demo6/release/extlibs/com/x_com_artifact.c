#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"

#include "internal/com_ole.h"

#ifdef __TINYC__
void WINAPI CoUninitialize(void);
HRESULT WINAPI CoCreateInstance(REFCLSID clsid, IUnknown* outer,
	DWORD context, REFIID iid, void** instance);
#endif

static const IID x_com_iid_null = { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };
static const IID x_com_iid_idispatch = { 0x00020400, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

#if defined(_WIN32) || defined(_WIN64)
#define XL_EXPORT __declspec(dllexport)
#else
#define XL_EXPORT
#endif

typedef struct XComObject {
	IDispatch* dispatch;
	DWORD owner_thread;
	int owns_apartment;
	char* text;
	size_t text_size;
	size_t text_capacity;
} XComObject;

static int g_x_com_missing_sentinel = 0;

static xvalue* x_com_variant_to_xvalue(VARIANT* value);


static void x_com_set_error(const char* text)
{
	if ( text != NULL && text[0] != '\0' ) {
		xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.com", 0, text);
	} else {
		xrtClearError();
	}
}

static void x_com_set_hresult_error(const char* prefix, HRESULT hr)
{
	char message[160];
	snprintf(message, sizeof(message), "%s: 0x%08lx", prefix, (unsigned long)hr);
	x_com_set_error(message);
}


static int x_com_begin_apartment(int* owns_apartment)
{
	HRESULT hr;
	*owns_apartment = 0;
	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if ( FAILED(hr) && hr != RPC_E_CHANGED_MODE ) {
		x_com_set_hresult_error("CoInitializeEx failed", hr);
		return 0;
	}
	*owns_apartment = SUCCEEDED(hr);
	return 1;
}
static int x_com_check_handle(const XComObject* object)
{
	if ( object == NULL || object->dispatch == NULL ) {
		x_com_set_error("invalid COM handle");
		return 0;
	}
	if ( object->owner_thread != GetCurrentThreadId() ) {
		x_com_set_error("COM handle belongs to another thread");
		return 0;
	}
	x_com_set_error("");
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
	n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1, dst, dst_count);
	if ( n <= 0 ) {
		dst[0] = 0;
		return 0;
	}
	dst[dst_count - 1] = 0;
	return 1;
}

static char* x_com_utf8_from_wide(const wchar_t* src, size_t length, size_t* output_size)
{
	int n;
	char* output;
	if ( output_size != NULL ) {
		*output_size = 0u;
	}
	if ( src == NULL || length == 0u ) {
		output = (char*)xrtMalloc(1u);
		if ( output != NULL ) {
			output[0] = '\0';
		}
		return output;
	}
	if ( length > INT_MAX ) {
		x_com_set_error("COM string is too long");
		return NULL;
	}
	n = WideCharToMultiByte(CP_UTF8, 0, src, (int)length, NULL, 0, NULL, NULL);
	if ( n <= 0 ) {
		x_com_set_error("COM UTF-16 conversion failed");
		return NULL;
	}
	output = (char*)xrtMalloc((size_t)n + 1u);
	if ( output == NULL ) {
		x_com_set_error("out of memory");
		return NULL;
	}
	if ( WideCharToMultiByte(CP_UTF8, 0, src, (int)length, output, n, NULL, NULL) != n ) {
		xrtFree(output);
		x_com_set_error("COM UTF-16 conversion failed");
		return NULL;
	}
	output[n] = '\0';
	if ( output_size != NULL ) {
		*output_size = (size_t)n;
	}
	return output;
}

static void x_com_finish_exception(EXCEPINFO* info, HRESULT hr, const char* prefix, int report)
{
	if ( info->pfnDeferredFillIn != NULL ) {
		info->pfnDeferredFillIn(info);
	}
	if ( report && FAILED(hr) ) {
		char* message = x_com_utf8_from_wide(info->bstrDescription,
			info->bstrDescription != NULL ? SysStringLen(info->bstrDescription) : 0u, NULL);
		if ( message != NULL && message[0] != '\0' ) {
			x_com_set_error(message);
		} else {
			x_com_set_hresult_error(prefix, hr);
		}
		xrtFree(message);
	}
	SysFreeString(info->bstrSource);
	SysFreeString(info->bstrDescription);
	SysFreeString(info->bstrHelpFile);
	memset(info, 0, sizeof(*info));
}

static xvalue* x_com_value_from_wide(BSTR text)
{
	size_t size;
	char* utf8 = x_com_utf8_from_wide(text, text != NULL ? SysStringLen(text) : 0u, &size);
	xvalue* value;
	if ( utf8 == NULL ) {
		return xrtValueNull();
	}
	value = xrtValueStringTake(&utf8, size);
	xrtFree(utf8);
	return value;
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
		size_t length = strlen(src);
		if ( length > INT_MAX ) {
			x_com_set_error("COM string is too long");
			return NULL;
		}
		src_len = (int)length;
	}
	if ( src_len == 0 ) {
		return SysAllocString(L"");
	}
	wide_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, src_len, NULL, 0);
	if ( wide_len <= 0 ) {
		x_com_set_error("invalid UTF-8 COM argument");
		return NULL;
	}
	wide = (wchar_t*)xrtMalloc((size_t)wide_len * sizeof(wchar_t));
	if ( wide == NULL ) {
		x_com_set_error("out of memory");
		return NULL;
	}
	if ( MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, src_len, wide, wide_len) != wide_len ) {
		xrtFree(wide);
		x_com_set_error("invalid UTF-8 COM argument");
		return NULL;
	}
	result = SysAllocStringLen(wide, (UINT)wide_len);
	xrtFree(wide);
	if ( result == NULL ) {
		x_com_set_error("out of memory");
	}
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
		x_com_finish_exception(&excep, hr, "Invoke failed", 0);
		VariantClear(result);
		VariantInit(result);
		hr = dispatch->lpVtbl->Invoke(dispatch, dispid, &x_com_iid_null, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, result, &excep, &arg_error);
	}
	x_com_finish_exception(&excep, hr, "Invoke failed", 1);
	if ( FAILED(hr) ) {
		VariantClear(result);
		return 0;
	}
	return 1;
}

static int x_com_variant_from_xvalue(VARIANT* dst, const xvalue* value)
{
	xvaluetype Type;
	bool bValue;
	int64 iValue;
	xstrview Text;
	ptr pValue;
	if ( dst == NULL ) {
		return 0;
	}
	VariantInit(dst);
	Type = xrtValueType(value);
	if ( Type == XVALUE_NULL ) {
		dst->vt = VT_EMPTY;
		return 1;
	}
	switch ( Type ) {
	case XVALUE_BOOL:
		dst->vt = VT_BOOL;
		if ( !xrtValueGetBool(value, &bValue) ) { return 0; }
		dst->boolVal = bValue ? VARIANT_TRUE : VARIANT_FALSE;
		return 1;
	case XVALUE_INT:
		if ( !xrtValueGetInt(value, &iValue) ) { return 0; }
		if ( iValue >= (int64)LONG_MIN && iValue <= (int64)LONG_MAX ) {
			dst->vt = VT_I4;
			dst->lVal = (LONG)iValue;
		} else {
			dst->vt = VT_I8;
			dst->llVal = iValue;
		}
		return 1;
	case XVALUE_FLOAT:
		dst->vt = VT_R8;
		return xrtValueGetFloat(value, &dst->dblVal) ? 1 : 0;
	case XVALUE_STRING:
		if ( !xrtValueGetString(value, &Text) ) { return 0; }
		if ( Text.Size > INT_MAX ) {
			x_com_set_error("COM string is too long");
			return 0;
		}
		dst->vt = VT_BSTR;
		dst->bstrVal = x_com_bstr_from_utf8_slice(Text.Data, (int)Text.Size);
		if ( dst->bstrVal == NULL ) {
			return 0;
		}
		return 1;
	case XVALUE_POINTER:
		if ( !xrtValueGetPointer(value, &pValue) ) { return 0; }
		if ( pValue == (void*)&g_x_com_missing_sentinel ) {
			dst->vt = VT_ERROR;
			dst->scode = DISP_E_PARAMNOTFOUND;
			return 1;
		}
		if ( pValue != NULL ) {
			dst->vt = VT_DISPATCH;
			dst->pdispVal = (IDispatch*)pValue;
			dst->pdispVal->lpVtbl->AddRef(dst->pdispVal);
			return 1;
		}
		dst->vt = VT_EMPTY;
		return 1;
	default:
		x_com_set_error("unsupported COM argument type");
		return 0;
	}
}

static int x_com_put_var_variant(IDispatch* dispatch, const char* name, const xvalue* value)
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
	x_com_finish_exception(&excep, hr, "Property put failed", 1);
	if ( FAILED(hr) ) {
		return 0;
	}
	return 1;
}

static int x_com_reserve_text(XComObject* object, size_t extra)
{
	size_t required;
	size_t capacity;
	char* output;
	if ( object == NULL || extra > SIZE_MAX - object->text_size - 1u ) {
		x_com_set_error("COM text size overflow");
		return 0;
	}
	required = object->text_size + extra + 1u;
	if ( required <= object->text_capacity ) {
		return 1;
	}
	capacity = object->text_capacity != 0u ? object->text_capacity : 64u;
	while ( capacity < required ) {
		if ( capacity > SIZE_MAX / 2u ) {
			capacity = required;
			break;
		}
		capacity *= 2u;
	}
	output = (char*)xrtRealloc(object->text, capacity);
	if ( output == NULL ) {
		x_com_set_error("out of memory");
		return 0;
	}
	object->text = output;
	object->text_capacity = capacity;
	return 1;
}

static void x_com_clear_text(XComObject* object)
{
	object->text_size = 0u;
	if ( object->text != NULL ) {
		object->text[0] = '\0';
	}
}

static int x_com_append_text(XComObject* object, const char* text, size_t size)
{
	if ( !x_com_reserve_text(object, size) ) {
		return 0;
	}
	if ( size != 0u ) {
		memcpy(object->text + object->text_size, text, size);
		object->text_size += size;
	}
	object->text[object->text_size] = '\0';
	return 1;
}

static int x_com_append_wide(XComObject* object, BSTR text)
{
	size_t size;
	char* utf8 = x_com_utf8_from_wide(text, text != NULL ? SysStringLen(text) : 0u, &size);
	int ok;
	if ( utf8 == NULL ) {
		return 0;
	}
	if ( memchr(utf8, 0, size) != NULL ) {
		x_com_set_error("COM text contains NUL; use invoke() for a value result");
		xrtFree(utf8);
		return 0;
	}
	ok = x_com_append_text(object, utf8, size);
	xrtFree(utf8);
	return ok;
}

static int x_com_append_variant_text(XComObject* object, VARIANT* value)
{
	VARIANT text;
	HRESULT hr;
	int ok;
	VariantInit(&text);
	if ( value == NULL || value->vt == VT_EMPTY || value->vt == VT_NULL ) {
		return 1;
	}
	hr = VariantChangeType(&text, value, 0, VT_BSTR);
	if ( FAILED(hr) ) {
		x_com_set_hresult_error("VariantChangeType string failed", hr);
		return 0;
	}
	ok = x_com_append_wide(object, text.bstrVal);
	VariantClear(&text);
	return ok;
}

static int x_com_safearray_to_text(XComObject* object, VARIANT* value)
{
	SAFEARRAY* array;
	VARTYPE item_vt;
	LONG lower;
	LONG upper;
	LONG index;
	if ( value == NULL || (value->vt & VT_ARRAY) == 0 ) {
		return 1;
	}
	array = (value->vt & VT_BYREF) ? *value->pparray : value->parray;
	if ( array == NULL ) {
		return 1;
	}
	if ( SafeArrayGetDim(array) != 1u ) {
		x_com_set_error("only one-dimensional SAFEARRAY is supported");
		return 0;
	}
	if ( FAILED(SafeArrayGetLBound(array, 1u, &lower)) || FAILED(SafeArrayGetUBound(array, 1u, &upper)) ) {
		x_com_set_error("SAFEARRAY bounds failed");
		return 0;
	}
	item_vt = value->vt & ~(VT_ARRAY | VT_BYREF);
	for ( index = lower; index <= upper; ) {
		VARIANT item;
		HRESULT hr;
		if ( index != lower ) {
			if ( !x_com_append_text(object, ",", 1u) ) {
				return 0;
			}
		}
		VariantInit(&item);
		if ( item_vt == VT_VARIANT || item_vt == VT_EMPTY ) {
			hr = SafeArrayGetElement(array, &index, &item);
			if ( FAILED(hr) ) {
				x_com_set_hresult_error("SAFEARRAY element read failed", hr);
				VariantClear(&item);
				return 0;
			}
			if ( !x_com_append_variant_text(object, &item) ) {
				VariantClear(&item);
				return 0;
			}
			VariantClear(&item);
		} else {
			item.vt = item_vt;
			hr = SafeArrayGetElement(array, &index, &item.byref);
			if ( FAILED(hr) ) {
				x_com_set_hresult_error("SAFEARRAY element read failed", hr);
				VariantClear(&item);
				return 0;
			}
			if ( !x_com_append_variant_text(object, &item) ) {
				VariantClear(&item);
				return 0;
			}
			VariantClear(&item);
		}
		if ( index == upper ) {
			break;
		}
		++index;
	}
	return 1;
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

static xvalue* x_com_safearray_to_xvalue_rec(
	SAFEARRAY* array,
	VARTYPE item_vt,
	UINT dim,
	UINT dim_count,
	LONG* lower,
	LONG* upper,
	LONG* indices
)
{
	xvalue* pResult;
	LONG index;

	pResult = xrtValueArray();
	if ( pResult == NULL ) {
		return xrtValueNull();
	}
	for ( index = lower[dim]; index <= upper[dim]; ) {
		xvalue* pItem;
		indices[dim] = index;
		if ( (dim + 1u) < dim_count ) {
			pItem = x_com_safearray_to_xvalue_rec(array, item_vt, dim + 1u, dim_count, lower, upper, indices);
		} else {
			VARIANT item;
			if ( x_com_safearray_get_variant(array, item_vt, indices, &item) ) {
				pItem = x_com_variant_to_xvalue(&item);
				VariantClear(&item);
			} else {
				VariantClear(&item);
				x_com_set_error("SAFEARRAY element read failed");
				xrtValueRelease(pResult);
				return xrtValueNull();
			}
		}
		if ( xrtGetError() != NULL ) {
			xrtValueRelease(pItem);
			xrtValueRelease(pResult);
			return xrtValueNull();
		}
		if ( !xrtValueArrayAppendNew(pResult, pItem) ) {
			xrtValueRelease(pResult);
			return xrtValueNull();
		}
		if ( index == upper[dim] ) {
			break;
		}
		++index;
	}
	return pResult;
}

static xvalue* x_com_safearray_to_xvalue(VARIANT* value)
{
	SAFEARRAY* array;
	UINT dim_count;
	VARTYPE item_vt;
	LONG lower[8];
	LONG upper[8];
	LONG indices[8];
	UINT dim;

	if ( value == NULL || (value->vt & VT_ARRAY) == 0 ) {
		return xrtValueNull();
	}
	array = (value->vt & VT_BYREF) ? *value->pparray : value->parray;
	if ( array == NULL ) {
		return xrtValueArray();
	}
	dim_count = SafeArrayGetDim(array);
	if ( dim_count == 0u ) {
		return xrtValueArray();
	}
	if ( dim_count > 8u ) {
		x_com_set_error("SAFEARRAY dimension count is too large");
		return xrtValueNull();
	}
	for ( dim = 0u; dim < dim_count; ++dim ) {
		if ( FAILED(SafeArrayGetLBound(array, dim + 1u, &lower[dim])) ||
			FAILED(SafeArrayGetUBound(array, dim + 1u, &upper[dim])) ) {
			x_com_set_error("SAFEARRAY bounds failed");
			return xrtValueNull();
		}
		indices[dim] = lower[dim];
	}
	item_vt = value->vt & ~(VT_ARRAY | VT_BYREF);
	return x_com_safearray_to_xvalue_rec(array, item_vt, 0u, dim_count, lower, upper, indices);
}

static const char* x_com_variant_to_text(XComObject* object, VARIANT* value)
{
	x_com_clear_text(object);
	if ( value == NULL || value->vt == VT_EMPTY || value->vt == VT_NULL ) {
		return "";
	}
	if ( (value->vt & VT_ARRAY) != 0 ) {
		return x_com_safearray_to_text(object, value)
			? (object->text != NULL ? object->text : "") : "";
	}
	return x_com_append_variant_text(object, value) ? object->text : "";
}

static xvalue* x_com_variant_to_xvalue(VARIANT* value)
{
	VARIANT text;
	VARTYPE vt;
	if ( value == NULL ) {
		return xrtValueNull();
	}
	while ( (value->vt & VT_BYREF) != 0 ) {
		vt = value->vt & ~VT_BYREF;
		if ( vt == VT_BOOL && value->pboolVal != NULL ) {
			return xrtValueBool((*value->pboolVal) != VARIANT_FALSE);
		}
		if ( vt == VT_I4 && value->plVal != NULL ) {
			return xrtValueInt((int64)(*value->plVal));
		}
		if ( vt == VT_I8 && value->pllVal != NULL ) {
			return xrtValueInt((int64)(*value->pllVal));
		}
		if ( vt == VT_R8 && value->pdblVal != NULL ) {
			return xrtValueFloat(*value->pdblVal);
		}
		if ( vt == VT_BSTR && value->pbstrVal != NULL ) {
			return x_com_value_from_wide(*value->pbstrVal);
		}
		return xrtValueNull();
	}
	if ( (value->vt & VT_ARRAY) != 0 ) {
		return x_com_safearray_to_xvalue(value);
	}
	switch ( value->vt ) {
	case VT_EMPTY:
	case VT_NULL:
		return xrtValueNull();
	case VT_BOOL:
		return xrtValueBool(value->boolVal != VARIANT_FALSE);
	case VT_I1:
		return xrtValueInt((int64)value->cVal);
	case VT_UI1:
		return xrtValueInt((int64)value->bVal);
	case VT_I2:
		return xrtValueInt((int64)value->iVal);
	case VT_UI2:
		return xrtValueInt((int64)value->uiVal);
	case VT_I4:
	case VT_INT:
		return xrtValueInt((int64)value->lVal);
	case VT_UI4:
	case VT_UINT:
		return xrtValueInt((int64)value->ulVal);
	case VT_I8:
		return xrtValueInt((int64)value->llVal);
	case VT_UI8:
		if ( value->ullVal > INT64_MAX ) {
			x_com_set_error("COM uint64 exceeds XLang int range");
			return xrtValueNull();
		}
		return xrtValueInt((int64)value->ullVal);
	case VT_R4:
		return xrtValueFloat((double)value->fltVal);
	case VT_R8:
		return xrtValueFloat(value->dblVal);
	case VT_BSTR:
		return x_com_value_from_wide(value->bstrVal);
	case VT_DISPATCH:
		if ( value->pdispVal != NULL ) {
			value->pdispVal->lpVtbl->AddRef(value->pdispVal);
		}
		return xrtValuePointer((ptr)value->pdispVal);
	case VT_UNKNOWN:
	{
		IDispatch* dispatch = NULL;
		if ( value->punkVal != NULL &&
			SUCCEEDED(value->punkVal->lpVtbl->QueryInterface(value->punkVal, &x_com_iid_idispatch, (void**)&dispatch)) ) {
			return xrtValuePointer((ptr)dispatch);
		}
		return xrtValueNull();
	}
	default:
		VariantInit(&text);
		if ( SUCCEEDED(VariantChangeType(&text, value, 0, VT_BSTR)) ) {
			xvalue* pResult;
			pResult = x_com_value_from_wide(text.bstrVal);
			VariantClear(&text);
			return pResult;
		}
		return xrtValueNull();
	}
}

XL_EXPORT void* x_com_create(const char* progid)
{
	wchar_t wide_progid[256];
	CLSID clsid;
	IDispatch* dispatch = NULL;
	XComObject* object;
	HRESULT hr;
	int owns_apartment;
	x_com_set_error("");
	if ( !x_com_begin_apartment(&owns_apartment) ) {
		return NULL;
	}
	if ( !x_com_utf8_to_wide(progid, wide_progid, (int)(sizeof(wide_progid) / sizeof(wide_progid[0]))) ) {
		x_com_set_error("ProgID conversion failed");
		goto fail;
	}
	hr = CLSIDFromProgID(wide_progid, &clsid);
	if ( FAILED(hr) ) {
		x_com_set_hresult_error("CLSIDFromProgID failed", hr);
		goto fail;
	}
	hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, &x_com_iid_idispatch, (void**)&dispatch);
	if ( FAILED(hr) ) {
		x_com_set_hresult_error("CoCreateInstance failed", hr);
		goto fail;
	}
	object = (XComObject*)xrtCalloc(1u, sizeof(*object));
	if ( object == NULL ) {
		dispatch->lpVtbl->Release(dispatch);
		x_com_set_error("out of memory");
		goto fail;
	}
	object->dispatch = dispatch;
	object->owner_thread = GetCurrentThreadId();
	object->owns_apartment = owns_apartment;
	return object;

fail:
	if ( owns_apartment ) {
		CoUninitialize();
	}
	return NULL;
}

XL_EXPORT void* x_com_attach(void* dispatch)
{
	XComObject* object;
	int owns_apartment;
	x_com_set_error("");
	if ( dispatch == NULL ) {
		x_com_set_error("COM dispatch pointer is null");
		return NULL;
	}
	if ( !x_com_begin_apartment(&owns_apartment) ) {
		return NULL;
	}
	object = (XComObject*)xrtCalloc(1u, sizeof(*object));
	if ( object == NULL ) {
		x_com_set_error("out of memory");
		if ( owns_apartment ) {
			CoUninitialize();
		}
		return NULL;
	}
	object->dispatch = (IDispatch*)dispatch;
	object->owner_thread = GetCurrentThreadId();
	object->owns_apartment = owns_apartment;
	return object;
}


XL_EXPORT int64_t x_com_available(void)
{
	int owns_apartment;
	x_com_set_error("");
	if ( !x_com_begin_apartment(&owns_apartment) ) {
		return 0;
	}
	if ( owns_apartment ) {
		CoUninitialize();
	}
	return 1;
}

XL_EXPORT const char* x_com_version(void)
{
	return "0.1.0";
}

XL_EXPORT int64_t x_com_release(void* handle)
{
	XComObject* object = (XComObject*)handle;
	if ( object == NULL ) {
		return 0;
	}
	if ( !x_com_check_handle(object) ) {
		return 0;
	}
	if ( object->dispatch != NULL ) {
		object->dispatch->lpVtbl->Release(object->dispatch);
		object->dispatch = NULL;
	}
	if ( object->owns_apartment ) {
		CoUninitialize();
	}
	xrtFree(object->text);
	xrtFree(object);
	return 1;
}

XL_EXPORT const char* x_com_last_error(void)
{
	const xerror* error = xrtGetError();
	return error != NULL && strcmp(xrtErrorDomain(error), "xlang.extlib.com") == 0
		? xrtErrorMessage(error) : "";
}

XL_EXPORT int64_t x_com_is_null(void* handle)
{
	return handle == NULL ? 1 : 0;
}

XL_EXPORT const char* x_com_get_text(void* handle, const char* name)
{
	XComObject* object = (XComObject*)handle;
	VARIANT result;
	const char* text;
	if ( !x_com_check_handle(object) ) {
		return "";
	}
	if ( !x_com_invoke0_variant(object->dispatch, name, DISPATCH_PROPERTYGET, &result) ) {
		return "";
	}
	text = x_com_variant_to_text(object, &result);
	VariantClear(&result);
	return text;
}

XL_EXPORT int64_t x_com_get_int(void* handle, const char* name)
{
	XComObject* object = (XComObject*)handle;
	VARIANT result;
	VARIANT number;
	int64_t value = 0;
	HRESULT hr;
	if ( !x_com_check_handle(object) ) {
		return 0;
	}
	if ( !x_com_invoke0_variant(object->dispatch, name, DISPATCH_PROPERTYGET, &result) ) {
		return 0;
	}
	VariantInit(&number);
	hr = VariantChangeType(&number, &result, 0, VT_I8);
	if ( SUCCEEDED(hr) ) {
		value = number.llVal;
	} else {
		x_com_set_hresult_error("VariantChangeType int failed", hr);
	}
	VariantClear(&number);
	VariantClear(&result);
	return value;
}

XL_EXPORT int64_t x_com_put_var(void* handle, const char* name, xvalue* value)
{
	XComObject* object = (XComObject*)handle;
	if ( !x_com_check_handle(object) ) {
		return 0;
	}
	return x_com_put_var_variant(object->dispatch, name, value);
}

XL_EXPORT void* x_com_missing(void)
{
	return (void*)&g_x_com_missing_sentinel;
}

static int x_com_value_truthy(const xvalue* pValue)
{
	xvaluetype Type = xrtValueType(pValue);
	bool bValue;
	int64 iValue;
	double fValue;
	xstrview Text;

	if ( Type == XVALUE_NULL ) {
		return 0;
	}
	if ( Type == XVALUE_BOOL ) {
		return xrtValueGetBool(pValue, &bValue) && bValue;
	}
	if ( Type == XVALUE_INT ) {
		return xrtValueGetInt(pValue, &iValue) && iValue != 0;
	}
	if ( Type == XVALUE_FLOAT ) {
		return xrtValueGetFloat(pValue, &fValue) && fValue != 0.0;
	}
	if ( Type == XVALUE_STRING ) {
		return xrtValueGetString(pValue, &Text) && Text.Size != 0u;
	}
	return 1;
}

static int x_com_resolve_call_names(IDispatch* dispatch, const char* method,
	const xvalue* names_value, size_t count, size_t* destinations,
	DISPID* method_id, DISPID* named_ids, UINT* named_count)
{
	BSTR names[65] = { 0 };
	DISPID ids[65];
	UINT used = 0u;
	size_t i;
	int ok = 0;
	HRESULT hr;

	if ( names_value == NULL ) {
		for ( i = 0u; i < count; ++i ) {
			destinations[i] = SIZE_MAX;
		}
		*named_count = 0u;
		return x_com_get_dispid(dispatch, method, method_id);
	}
	names[0] = x_com_bstr_from_utf8_slice(method, -1);
	if ( names[0] == NULL ) {
		return 0;
	}
	for ( i = 0u; i < count; ++i ) {
		const xvalue* item = names_value != NULL ? xrtValueArrayGet(names_value, i) : NULL;
		xstrview text;
		if ( item == NULL || xrtValueType(item) == XVALUE_NULL ) {
			destinations[i] = SIZE_MAX;
			continue;
		}
		if ( !xrtValueGetString(item, &text) ) {
			x_com_set_error("named argument must be a string or null");
			goto done;
		}
		if ( text.Size == 0u ) {
			destinations[i] = SIZE_MAX;
			continue;
		}
		if ( text.Size > INT_MAX || memchr(text.Data, 0, text.Size) != NULL ) {
			x_com_set_error("invalid named COM argument");
			goto done;
		}
		names[used + 1u] = x_com_bstr_from_utf8_slice(text.Data, (int)text.Size);
		if ( names[used + 1u] == NULL ) {
			goto done;
		}
		destinations[i] = used++;
	}
	hr = dispatch->lpVtbl->GetIDsOfNames(dispatch, &x_com_iid_null,
		(LPOLESTR*)names, used + 1u, LOCALE_USER_DEFAULT, ids);
	if ( FAILED(hr) ) {
		x_com_set_hresult_error("GetIDsOfNames failed", hr);
		goto done;
	}
	for ( i = 0u; i < used; ++i ) {
		size_t j;
		for ( j = 0u; j < i; ++j ) {
			if ( ids[i + 1u] == ids[j + 1u] ) {
				x_com_set_error("duplicate named COM argument");
				goto done;
			}
		}
		named_ids[i] = ids[i + 1u];
	}
	*method_id = ids[0];
	*named_count = used;
	ok = 1;
done:
	for ( i = 0u; i <= used; ++i ) {
		SysFreeString(names[i]);
	}
	return ok;
}

static xvalue* x_com_invoke_array_internal(
	void* handle,
	const char* name,
	const xvalue* pArgs,
	const xvalue* pNames,
	const xvalue* pByRefs,
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
	size_t destinations[64];
	VARIANT result;
	size_t iCount;
	size_t i;
	size_t positional_count = 0u;
	UINT named_count = 0u;
	HRESULT hr;
	xvalue* pOutput;

	if ( !x_com_check_handle(object) ) {
		return xrtValueNull();
	}
	iCount = pArgs != NULL ? xrtValueCount(pArgs) : 0u;
	if ( pArgs != NULL && xrtValueType(pArgs) != XVALUE_ARRAY ) {
		x_com_set_error("arguments must be an array");
		return xrtValueNull();
	}
	if ( iCount > 64u ) {
		x_com_set_error("unsupported argument count");
		return xrtValueNull();
	}
	if ( pNames != NULL &&
		(xrtValueType(pNames) != XVALUE_ARRAY || xrtValueCount(pNames) != iCount) ) {
		x_com_set_error("named arguments must match argument count");
		return xrtValueNull();
	}
	if ( pByRefs != NULL &&
		(xrtValueType(pByRefs) != XVALUE_ARRAY || xrtValueCount(pByRefs) != iCount) ) {
		x_com_set_error("byref flags must match argument count");
		return xrtValueNull();
	}
	memset(named_ids, 0, sizeof(named_ids));
	for ( i = 0u; i < 64u; ++i ) {
		VariantInit(&invoke_args[i]);
		VariantInit(&holders[i]);
	}
	if ( !x_com_resolve_call_names(object->dispatch, name, pNames, iCount,
		destinations, &dispid, named_ids, &named_count) ) {
		goto fail;
	}
	for ( i = 0u; i < iCount; ++i ) {
		size_t iDestination = destinations[i];
		const xvalue* pItem = pArgs != NULL ? xrtValueArrayGet(pArgs, i) : NULL;
		const xvalue* pByRef = pByRefs != NULL ? xrtValueArrayGet(pByRefs, i) : NULL;
		if ( iDestination == SIZE_MAX ) {
			iDestination = iCount - 1u - positional_count++;
		}
		if ( !x_com_variant_from_xvalue(&holders[i], pItem) ) {
			goto fail;
		}
		if ( x_com_value_truthy(pByRef) ) {
			invoke_args[iDestination].vt = VT_VARIANT | VT_BYREF;
			invoke_args[iDestination].pvarVal = &holders[i];
		} else {
			invoke_args[iDestination] = holders[i];
			VariantInit(&holders[i]);
		}
	}
	memset(&params, 0, sizeof(params));
	memset(&excep, 0, sizeof(excep));
	VariantInit(&result);
	params.cArgs = (UINT)iCount;
	params.rgvarg = (iCount != 0u) ? invoke_args : NULL;
	params.cNamedArgs = named_count;
	params.rgdispidNamedArgs = (named_count != 0u) ? named_ids : NULL;
	hr = object->dispatch->lpVtbl->Invoke(object->dispatch, dispid, &x_com_iid_null, LOCALE_USER_DEFAULT, flags, &params, &result, &excep, &arg_error);
	if ( FAILED(hr) && (flags & DISPATCH_PROPERTYGET) != 0 ) {
		x_com_finish_exception(&excep, hr, "Invoke failed", 0);
		VariantClear(&result);
		VariantInit(&result);
		hr = object->dispatch->lpVtbl->Invoke(object->dispatch, dispid, &x_com_iid_null, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, &result, &excep, &arg_error);
	}
	x_com_finish_exception(&excep, hr, "Invoke failed", 1);
	if ( FAILED(hr) ) {
		VariantClear(&result);
		goto fail;
	}
	if ( bReturnOutArgs ) {
		pOutput = xrtValueArray();
		if ( pOutput == NULL ) {
			pOutput = xrtValueNull();
		} else {
			xrtValueArrayAppendNew(pOutput, x_com_variant_to_xvalue(&result));
			for ( i = 0u; i < iCount; ++i ) {
				xrtValueArrayAppendNew(pOutput, x_com_variant_to_xvalue(&holders[i]));
			}
		}
	} else {
		pOutput = x_com_variant_to_xvalue(&result);
	}
	VariantClear(&result);
	for ( i = 0u; i < iCount; ++i ) {
		VariantClear(&invoke_args[i]);
		VariantClear(&holders[i]);
	}
	return pOutput;

fail:
	for ( i = 0u; i < iCount; ++i ) {
		VariantClear(&invoke_args[i]);
		VariantClear(&holders[i]);
	}
	return xrtValueNull();
}

XL_EXPORT xvalue* x_com_invoke_array(void* handle, const char* name, xvalue* pArgs)
{
	return x_com_invoke_array_internal(handle, name, pArgs, NULL, NULL, DISPATCH_METHOD | DISPATCH_PROPERTYGET, 0);
}

XL_EXPORT xvalue* x_com_invoke_out(void* handle, const char* name, xvalue* pArgs, xvalue* pByRefs)
{
	return x_com_invoke_array_internal(handle, name, pArgs, NULL, pByRefs, DISPATCH_METHOD | DISPATCH_PROPERTYGET, 1);
}

XL_EXPORT xvalue* x_com_invoke_named(void* handle, const char* name, xvalue* pArgs, xvalue* pNames)
{
	return x_com_invoke_array_internal(handle, name, pArgs, pNames, NULL, DISPATCH_METHOD | DISPATCH_PROPERTYGET, 0);
}

XL_EXPORT int64_t x_com_put_ref(void* handle, const char* name, xvalue* value)
{
	XComObject* object = (XComObject*)handle;
	DISPID dispid;
	DISPID named_arg = DISPID_PROPERTYPUT;
	DISPPARAMS params;
	EXCEPINFO excep;
	UINT arg_error = 0;
	VARIANT arg;
	HRESULT hr;

	if ( !x_com_check_handle(object) ) {
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
	x_com_finish_exception(&excep, hr, "Property putref failed", 1);
	if ( FAILED(hr) ) {
		return 0;
	}
	return 1;
}
