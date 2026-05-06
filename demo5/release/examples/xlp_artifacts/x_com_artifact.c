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

static int x_com_invoke_text_args_variant(IDispatch* dispatch, const char* name, int argc, const char** argv, VARIANT* result)
{
	DISPID dispid;
	DISPPARAMS params;
	EXCEPINFO excep;
	UINT arg_error = 0;
	VARIANT args[8];
	int i;
	HRESULT hr;
	if ( result == NULL ) {
		return 0;
	}
	VariantInit(result);
	if ( argc < 0 || argc > 8 ) {
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

static const char* x_com_variant_to_text(VARIANT* value)
{
	VARIANT text;
	HRESULT hr;
	VariantInit(&text);
	if ( value == NULL || value->vt == VT_EMPTY || value->vt == VT_NULL ) {
		g_x_com_text[0] = '\0';
		return g_x_com_text;
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

XL_EXPORT const char* x_com_invoke_text1(void* handle, const char* name, const char* a0)
{
	const char* args[1] = { a0 };
	return x_com_invoke_text_args(handle, name, 1, args);
}

XL_EXPORT const char* x_com_invoke_text2(void* handle, const char* name, const char* a0, const char* a1)
{
	const char* args[2] = { a0, a1 };
	return x_com_invoke_text_args(handle, name, 2, args);
}

XL_EXPORT const char* x_com_invoke_text3(void* handle, const char* name, const char* a0, const char* a1, const char* a2)
{
	const char* args[3] = { a0, a1, a2 };
	return x_com_invoke_text_args(handle, name, 3, args);
}

XL_EXPORT const char* x_com_invoke_text4(void* handle, const char* name, const char* a0, const char* a1, const char* a2, const char* a3)
{
	const char* args[4] = { a0, a1, a2, a3 };
	return x_com_invoke_text_args(handle, name, 4, args);
}

XL_EXPORT const char* x_com_invoke_text5(void* handle, const char* name, const char* a0, const char* a1, const char* a2, const char* a3, const char* a4)
{
	const char* args[5] = { a0, a1, a2, a3, a4 };
	return x_com_invoke_text_args(handle, name, 5, args);
}

XL_EXPORT const char* x_com_invoke_text6(void* handle, const char* name, const char* a0, const char* a1, const char* a2, const char* a3, const char* a4, const char* a5)
{
	const char* args[6] = { a0, a1, a2, a3, a4, a5 };
	return x_com_invoke_text_args(handle, name, 6, args);
}

XL_EXPORT const char* x_com_invoke_text7(void* handle, const char* name, const char* a0, const char* a1, const char* a2, const char* a3, const char* a4, const char* a5, const char* a6)
{
	const char* args[7] = { a0, a1, a2, a3, a4, a5, a6 };
	return x_com_invoke_text_args(handle, name, 7, args);
}

XL_EXPORT const char* x_com_invoke_text8(void* handle, const char* name, const char* a0, const char* a1, const char* a2, const char* a3, const char* a4, const char* a5, const char* a6, const char* a7)
{
	const char* args[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
	return x_com_invoke_text_args(handle, name, 8, args);
}
