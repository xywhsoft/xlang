#include <windows.h>
#include <oleauto.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct XL_ArtifactXValueStruct {
	union {
		struct {
			XL_ArtifactUInt32 Type:4;
			XL_ArtifactUInt32 Reserve:1;
			XL_ArtifactUInt32 IsStatic:1;
			XL_ArtifactUInt32 RefCount:26;
		};
		volatile XL_ArtifactUInt32 Header;
	};
	XL_ArtifactUInt32 Size;
	union {
		int vBool;
		XL_ArtifactInt64 vInt;
		double vFloat;
		XL_ArtifactStr vText;
		XL_ArtifactPtr vPoint;
	};
} *XL_ArtifactXValue;

#define XL_ARTIFACT_XVO_DT_NULL 0
#define XL_ARTIFACT_XVO_DT_BOOL 1
#define XL_ARTIFACT_XVO_DT_INT 2
#define XL_ARTIFACT_XVO_DT_FLOAT 3
#define XL_ARTIFACT_XVO_DT_TEXT 4
#define XL_ARTIFACT_XVO_DT_POINT 6

#define XL_ARTIFACT_XVO_HEADER_STATIC_MASK 0x00000020u
#define XL_ARTIFACT_XVO_HEADER_REFCOUNT_SHIFT 6u
#define XL_ARTIFACT_XVO_HEADER_INIT(type, is_static, ref_count) \
	((((XL_ArtifactUInt32)(type)) & 0x0Fu) | ((is_static) ? XL_ARTIFACT_XVO_HEADER_STATIC_MASK : 0u) | ((((XL_ArtifactUInt32)(ref_count)) & 0x03FFFFFFu) << XL_ARTIFACT_XVO_HEADER_REFCOUNT_SHIFT))

static struct XL_ArtifactXValueStruct g_x_com_null_value = { { { XL_ARTIFACT_XVO_DT_NULL, 0, 1, 0 } }, 0, { 0 } };
static struct XL_ArtifactXValueStruct g_x_com_true_value = { { { XL_ARTIFACT_XVO_DT_BOOL, 0, 1, 0 } }, sizeof(int), { 1 } };
static struct XL_ArtifactXValueStruct g_x_com_false_value = { { { XL_ARTIFACT_XVO_DT_BOOL, 0, 1, 0 } }, sizeof(int), { 0 } };
static struct XL_ArtifactXValueStruct g_x_com_return_value;
static char g_x_com_return_text[8192];

static XL_ArtifactXValue xvoCreateNull(void)
{
	return &g_x_com_null_value;
}

static XL_ArtifactXValue xvoCreateBool(int bVal)
{
	return bVal ? &g_x_com_true_value : &g_x_com_false_value;
}

static XL_ArtifactXValue xvoCreateInt(XL_ArtifactInt64 iVal)
{
	memset(&g_x_com_return_value, 0, sizeof(g_x_com_return_value));
	g_x_com_return_value.Header = XL_ARTIFACT_XVO_HEADER_INIT(XL_ARTIFACT_XVO_DT_INT, 1, 0);
	g_x_com_return_value.Size = sizeof(XL_ArtifactInt64);
	g_x_com_return_value.vInt = iVal;
	return &g_x_com_return_value;
}

static XL_ArtifactXValue xvoCreateFloat(double fVal)
{
	memset(&g_x_com_return_value, 0, sizeof(g_x_com_return_value));
	g_x_com_return_value.Header = XL_ARTIFACT_XVO_HEADER_INIT(XL_ARTIFACT_XVO_DT_FLOAT, 1, 0);
	g_x_com_return_value.Size = sizeof(double);
	g_x_com_return_value.vFloat = fVal;
	return &g_x_com_return_value;
}

static XL_ArtifactXValue xvoCreateText(void* sVal, XL_ArtifactUInt32 iSize, int bColloc)
{
	(void)bColloc;
	memset(&g_x_com_return_value, 0, sizeof(g_x_com_return_value));
	memset(g_x_com_return_text, 0, sizeof(g_x_com_return_text));
	if ( iSize >= sizeof(g_x_com_return_text) ) {
		iSize = (XL_ArtifactUInt32)(sizeof(g_x_com_return_text) - 1u);
	}
	if ( sVal != NULL && iSize != 0u ) {
		memcpy(g_x_com_return_text, sVal, iSize);
	}
	g_x_com_return_value.Header = XL_ARTIFACT_XVO_HEADER_INIT(XL_ARTIFACT_XVO_DT_TEXT, 1, 0);
	g_x_com_return_value.Size = iSize;
	g_x_com_return_value.vText = g_x_com_return_text;
	return &g_x_com_return_value;
}

static XL_ArtifactXValue xvoCreatePoint(void* point)
{
	memset(&g_x_com_return_value, 0, sizeof(g_x_com_return_value));
	g_x_com_return_value.Header = XL_ARTIFACT_XVO_HEADER_INIT(XL_ARTIFACT_XVO_DT_POINT, 1, 0);
	g_x_com_return_value.Size = sizeof(void*);
	g_x_com_return_value.vPoint = point;
	return &g_x_com_return_value;
}

static char g_x_com_text[4096];
static char g_x_com_error[512];
static int g_x_com_initialized = 0;

static void x_com_set_error(const char* text)
{
	if ( text == NULL ) {
		text = "";
	}
	strncpy(g_x_com_error, text, sizeof(g_x_com_error) - 1);
	g_x_com_error[sizeof(g_x_com_error) - 1] = '\0';
}

static void x_com_set_hresult_error(const char* prefix, HRESULT hr)
{
	snprintf(g_x_com_error, sizeof(g_x_com_error), "%s: 0x%08lx", prefix, (unsigned long)hr);
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
	hr = dispatch->lpVtbl->GetIDsOfNames(dispatch, &IID_NULL, names, 1, LOCALE_USER_DEFAULT, dispid);
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
	hr = dispatch->lpVtbl->Invoke(dispatch, dispid, &IID_NULL, LOCALE_USER_DEFAULT, flags, &params, result, &excep, &arg_error);
	if ( FAILED(hr) && (flags & DISPATCH_PROPERTYGET) != 0 ) {
		VariantClear(result);
		VariantInit(result);
		hr = dispatch->lpVtbl->Invoke(dispatch, dispid, &IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, result, &excep, &arg_error);
	}
	if ( FAILED(hr) ) {
		x_com_set_hresult_error("Invoke failed", hr);
		return 0;
	}
	return 1;
}

static int x_com_variant_from_xvalue(VARIANT* dst, XL_ArtifactXValue value)
{
	wchar_t wide_value[1024];
	char point_text[64];
	if ( dst == NULL ) {
		return 0;
	}
	VariantInit(dst);
	if ( value == NULL || value->Type == XL_ARTIFACT_XVO_DT_NULL ) {
		dst->vt = VT_EMPTY;
		return 1;
	}
	switch ( value->Type ) {
	case XL_ARTIFACT_XVO_DT_BOOL:
		dst->vt = VT_BOOL;
		dst->boolVal = value->vBool ? VARIANT_TRUE : VARIANT_FALSE;
		return 1;
	case XL_ARTIFACT_XVO_DT_INT:
		if ( value->vInt >= (XL_ArtifactInt64)LONG_MIN && value->vInt <= (XL_ArtifactInt64)LONG_MAX ) {
			dst->vt = VT_I4;
			dst->lVal = (LONG)value->vInt;
		} else {
			dst->vt = VT_I8;
			dst->llVal = value->vInt;
		}
		return 1;
	case XL_ARTIFACT_XVO_DT_FLOAT:
		dst->vt = VT_R8;
		dst->dblVal = value->vFloat;
		return 1;
	case XL_ARTIFACT_XVO_DT_TEXT:
		dst->vt = VT_BSTR;
		dst->bstrVal = x_com_bstr_from_utf8_slice(value->vText, (int)value->Size);
		if ( dst->bstrVal == NULL ) {
			x_com_set_error("argument allocation failed");
			return 0;
		}
		return 1;
	case XL_ARTIFACT_XVO_DT_POINT:
		snprintf(point_text, sizeof(point_text), "%p", value->vPoint);
		x_com_utf8_to_wide(point_text, wide_value, (int)(sizeof(wide_value) / sizeof(wide_value[0])));
		dst->vt = VT_BSTR;
		dst->bstrVal = SysAllocString(wide_value);
		if ( dst->bstrVal == NULL ) {
			x_com_set_error("argument allocation failed");
			return 0;
		}
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
	hr = dispatch->lpVtbl->Invoke(dispatch, dispid, &IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, result, &excep, &arg_error);
	if ( FAILED(hr) ) {
		VariantClear(result);
		VariantInit(result);
		memset(&excep, 0, sizeof(excep));
		arg_error = 0;
		hr = dispatch->lpVtbl->Invoke(dispatch, dispid, &IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &params, result, &excep, &arg_error);
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
	hr = dispatch->lpVtbl->Invoke(dispatch, dispid, &IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, result, &excep, &arg_error);
	if ( FAILED(hr) ) {
		VariantClear(result);
		VariantInit(result);
		memset(&excep, 0, sizeof(excep));
		arg_error = 0;
		hr = dispatch->lpVtbl->Invoke(dispatch, dispid, &IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &params, result, &excep, &arg_error);
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
	hr = dispatch->lpVtbl->Invoke(dispatch, dispid, &IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT, &params, NULL, &excep, &arg_error);
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
		array_text = x_com_safearray_to_text(value);
		return xvoCreateText((void*)array_text, (XL_ArtifactUInt32)strlen(array_text), 0);
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
			SUCCEEDED(value->punkVal->lpVtbl->QueryInterface(value->punkVal, &IID_IDispatch, (void**)&dispatch)) ) {
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
	hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, &IID_IDispatch, (void**)&dispatch);
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
