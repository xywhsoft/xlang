#include "x_uia_min.h"

#include "xrt.h"
#include "include/uia.h"

#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifndef WC_ERR_INVALID_CHARS
#define WC_ERR_INVALID_CHARS 0x80
#endif
#if defined(__TINYC__)
/* 内嵌 TCC 的 OLE 头缺少这两个公开 Win32 原型。 */
UINT WINAPI SysStringLen(BSTR value);
HRESULT WINAPI CoCreateInstance(const CLSID* clsid, IUnknown* outer,
	DWORD context, REFIID iid, void** result);
#endif

#if defined(_WIN32) || defined(_WIN64)
#define XL_EXPORT __declspec(dllexport)
#else
#define XL_EXPORT
#endif

#define X_UIA_VERSION "0.1.0"
#define X_UIA_MAGIC_PROCESS UINT32_C(0x58554950)
#define X_UIA_MAGIC_TEST_WINDOW UINT32_C(0x58554957)
#define X_UIA_MAGIC_ELEMENT UINT32_C(0x58554945)
#define X_UIA_MAGIC_CONDITION UINT32_C(0x58554943)
#define X_UIA_MAGIC_EVENTS UINT32_C(0x58554951)
#define X_UIA_EVENT_QUEUE_CAPACITY 64
#define X_UIA_JSON_MAX_BYTES (16u * 1024u * 1024u)
#define X_UIA_TEST_WINDOW_FOCUS_EDIT (WM_APP + 10u)
#define X_UIA_TEST_WINDOW_FOCUS_BUTTON (WM_APP + 11u)
#define X_UIA_CONDITION_TRUE 0
#define X_UIA_CONDITION_NAME 1
#define X_UIA_CONDITION_AUTOMATION_ID 2
#define X_UIA_CONDITION_CLASS_NAME 3
#define X_UIA_CONDITION_CONTROL_TYPE 4
#define X_UIA_CONDITION_PROCESS_ID 5
#define X_UIA_CONDITION_AND 6
#define X_UIA_CONDITION_OR 7
#define X_UIA_CONDITION_NOT 8

typedef struct XUiaTestWindow {
	uint32_t magic;
	HANDLE thread;
	HANDLE ready_event;
	HWND hwnd;
	HWND edit_hwnd;
	HWND button_hwnd;
	HWND checkbox_hwnd;
	DWORD thread_id;
} XUiaTestWindow;

typedef struct XUiaProcess {
	uint32_t magic;
	HANDLE process;
	HANDLE thread;
	DWORD pid;
} XUiaProcess;

typedef struct XUiaElement {
	uint32_t magic;
	IUIAutomationElement* element;
} XUiaElement;

typedef struct XUiaCondition {
	uint32_t magic;
	IUIAutomationCondition* condition;
} XUiaCondition;

typedef struct XUiaJsonBuilder {
	char* data;
	size_t capacity;
	size_t length;
	int overflow;
} XUiaJsonBuilder;

typedef struct XUiaSnapshotContext {
	XUiaJsonBuilder json;
	IUIAutomationTreeWalker* walker;
	int max_nodes;
	int node_count;
	int truncated;
} XUiaSnapshotContext;

typedef struct XUiaLocatorInfo {
	char name[512];
	char automation_id[512];
	char class_name[512];
	char class_path[2048];
	int control_type;
	int process_id;
	int sibling_index;
	UIA_HWND hwnd;
	RECT rect;
} XUiaLocatorInfo;

typedef struct XUiaEventRecord {
	char type[16];
	int property_id;
	int control_type;
	UIA_HWND hwnd;
} XUiaEventRecord;

typedef struct XUiaEvents XUiaEvents;

typedef struct XUiaFocusHandler {
	CONST_VTBL IUIAutomationFocusChangedEventHandlerVtbl* lpVtbl;
	volatile LONG ref_count;
	XUiaEvents* queue;
} XUiaFocusHandler;

typedef struct XUiaPropertyHandler {
	CONST_VTBL IUIAutomationPropertyChangedEventHandlerVtbl* lpVtbl;
	volatile LONG ref_count;
	XUiaEvents* queue;
} XUiaPropertyHandler;

struct XUiaEvents {
	uint32_t magic;
	volatile LONG ref_count;
	volatile LONG closing;
	CRITICAL_SECTION lock;
	HANDLE signal;
	XUiaEventRecord records[X_UIA_EVENT_QUEUE_CAPACITY];
	int head;
	int count;
	int overflow_count;
	int focus_subscribed;
	int property_subscribed;
	PROPERTYID property_id;
	IUIAutomationElement* property_element;
	XUiaFocusHandler* focus_handler;
	XUiaPropertyHandler* property_handler;
};

static IUIAutomation* g_x_uia = NULL;
static int g_x_uia_com_initialized = 0;
static volatile LONG g_x_uia_owner_thread = 0;
static volatile LONG g_x_uia_live_handles = 0;
static char g_x_uia_error[512] = "";
static char g_x_uia_text[8192] = "";
static char* g_x_uia_bstr_text = NULL;
static size_t g_x_uia_bstr_capacity = 0u;
static char* g_x_uia_json = NULL;
static size_t g_x_uia_json_capacity = 0u;
static const IID X_UIA_IID_IUnknown = { 0x00000000, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

static void x_uia_events_drop_ref(XUiaEvents* queue)
{
	if ( InterlockedDecrement(&queue->ref_count) != 0 ) return;
	if ( queue->signal != NULL ) CloseHandle(queue->signal);
	DeleteCriticalSection(&queue->lock);
	free(queue);
	InterlockedDecrement(&g_x_uia_live_handles);
}

XL_EXPORT int x_uia_shutdown(void);
static HRESULT STDMETHODCALLTYPE x_uia_focus_handler_query_interface(IUIAutomationFocusChangedEventHandler* self, REFIID riid, void** out);
static ULONG STDMETHODCALLTYPE x_uia_focus_handler_add_ref(IUIAutomationFocusChangedEventHandler* self);
static ULONG STDMETHODCALLTYPE x_uia_focus_handler_release(IUIAutomationFocusChangedEventHandler* self);
static HRESULT STDMETHODCALLTYPE x_uia_focus_handler_handle(IUIAutomationFocusChangedEventHandler* self, IUIAutomationElement* sender);
static HRESULT STDMETHODCALLTYPE x_uia_property_handler_query_interface(IUIAutomationPropertyChangedEventHandler* self, REFIID riid, void** out);
static ULONG STDMETHODCALLTYPE x_uia_property_handler_add_ref(IUIAutomationPropertyChangedEventHandler* self);
static ULONG STDMETHODCALLTYPE x_uia_property_handler_release(IUIAutomationPropertyChangedEventHandler* self);
static HRESULT STDMETHODCALLTYPE x_uia_property_handler_handle(IUIAutomationPropertyChangedEventHandler* self, IUIAutomationElement* sender, PROPERTYID property_id, VARIANT new_value);

static const IUIAutomationFocusChangedEventHandlerVtbl g_x_uia_focus_handler_vtbl = {
	x_uia_focus_handler_query_interface,
	x_uia_focus_handler_add_ref,
	x_uia_focus_handler_release,
	x_uia_focus_handler_handle
};

static const IUIAutomationPropertyChangedEventHandlerVtbl g_x_uia_property_handler_vtbl = {
	x_uia_property_handler_query_interface,
	x_uia_property_handler_add_ref,
	x_uia_property_handler_release,
	x_uia_property_handler_handle
};

static LRESULT CALLBACK x_uia_test_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	(void)lparam;
	if ( msg == WM_CLOSE ) {
		DestroyWindow(hwnd);
		return 0;
	}
	if ( msg == WM_COMMAND ) {
		XUiaTestWindow* win = (XUiaTestWindow*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
		if ( (LOWORD(wparam) == 1002u) && (win != NULL) && (win->edit_hwnd != NULL) ) {
			SetWindowTextA(win->edit_hwnd, "xlang-uia-button-clicked");
			return 0;
		}
	}
	if ( msg == X_UIA_TEST_WINDOW_FOCUS_EDIT ) {
		XUiaTestWindow* win = (XUiaTestWindow*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
		if ( (win != NULL) && (win->edit_hwnd != NULL) ) {
			SetForegroundWindow(hwnd);
			SetFocus(win->edit_hwnd);
		}
		return 0;
	}
	if ( msg == X_UIA_TEST_WINDOW_FOCUS_BUTTON ) {
		XUiaTestWindow* win = (XUiaTestWindow*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
		if ( (win != NULL) && (win->button_hwnd != NULL) ) {
			SetForegroundWindow(hwnd);
			SetFocus(win->button_hwnd);
		}
		return 0;
	}
	if ( msg == WM_DESTROY ) {
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static DWORD WINAPI x_uia_test_window_thread(void* param)
{
	XUiaTestWindow* win = (XUiaTestWindow*)param;
	WNDCLASSA wc;
	MSG msg;

	memset(&wc, 0, sizeof(wc));
	wc.lpfnWndProc = x_uia_test_window_proc;
	wc.hInstance = GetModuleHandleA(NULL);
	wc.lpszClassName = "XLangUiaTestWindow";
	RegisterClassA(&wc);
	win->hwnd = CreateWindowExA(
		0,
		"XLangUiaTestWindow",
		"xlang uia test",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		520,
		220,
		NULL,
		NULL,
		GetModuleHandleA(NULL),
		NULL
	);
	if ( win->hwnd != NULL ) {
		SetWindowLongPtrA(win->hwnd, GWLP_USERDATA, (LONG_PTR)win);
		win->edit_hwnd = CreateWindowExA(
			WS_EX_CLIENTEDGE,
			"EDIT",
			"",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL,
			20,
			40,
			460,
			28,
			win->hwnd,
			(HMENU)1001,
			GetModuleHandleA(NULL),
			NULL
		);
		win->button_hwnd = CreateWindowExA(
			0,
			"BUTTON",
			"Click",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
			20,
			82,
			120,
			32,
			win->hwnd,
			(HMENU)1002,
			GetModuleHandleA(NULL),
			NULL
		);
		win->checkbox_hwnd = CreateWindowExA(
			0,
			"BUTTON",
			"Toggle",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
			160,
			82,
			120,
			32,
			win->hwnd,
			(HMENU)1003,
			GetModuleHandleA(NULL),
			NULL
		);
		ShowWindow(win->hwnd, SW_SHOWNORMAL);
		UpdateWindow(win->hwnd);
	}
	SetEvent(win->ready_event);
	while ( GetMessageA(&msg, NULL, 0, 0) > 0 ) {
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
	return 0;
}

static XUiaTestWindow* x_uia_test_window_from_handle(void* handle)
{
	XUiaTestWindow* win = (XUiaTestWindow*)handle;
	if ( (win == NULL) || (win->magic != X_UIA_MAGIC_TEST_WINDOW) ) {
		return NULL;
	}
	return win;
}

static XUiaProcess* x_uia_process_from_handle(void* handle)
{
	XUiaProcess* process = (XUiaProcess*)handle;
	if ( (process == NULL) || (process->magic != X_UIA_MAGIC_PROCESS) ) {
		return NULL;
	}
	return process;
}

static void x_uia_copy_text(char* dst, size_t dst_size, const char* text)
{
	size_t len;

	if ( (dst == NULL) || (dst_size == 0u) ) {
		return;
	}
	if ( text == NULL ) {
		text = "";
	}
	len = strlen(text);
	if ( len >= dst_size ) {
		len = dst_size - 1u;
	}
	memcpy(dst, text, len);
	dst[len] = '\0';
}

static void x_uia_append_text(char* dst, size_t dst_size, const char* text)
{
	size_t used;
	size_t len;
	size_t space;

	if ( (dst == NULL) || (dst_size == 0u) ) {
		return;
	}
	if ( text == NULL ) {
		text = "";
	}
	used = strlen(dst);
	if ( used >= dst_size - 1u ) {
		dst[dst_size - 1u] = '\0';
		return;
	}
	space = dst_size - used - 1u;
	len = strlen(text);
	if ( len > space ) {
		len = space;
	}
	memcpy(dst + used, text, len);
	dst[used + len] = '\0';
}

static void x_uia_set_error(const char* text)
{
	LONG owner = InterlockedCompareExchange(&g_x_uia_owner_thread, 0, 0);
	if ( owner != 0 && (DWORD)owner != GetCurrentThreadId() ) {
		xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.uia", 1, "UIA context belongs to another thread");
		return;
	}
	x_uia_copy_text(g_x_uia_error, sizeof(g_x_uia_error), text);
	if ( text != NULL && text[0] != '\0' ) {
		xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.uia", 1, text);
	} else {
		xrtClearError();
	}
}

static void x_uia_set_hresult_error(const char* op, HRESULT hr)
{
	if ( InterlockedCompareExchange(&g_x_uia_owner_thread, 0, 0) != (LONG)GetCurrentThreadId() ) {
		xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.uia", 1, "UIA context belongs to another thread");
		return;
	}
	snprintf(g_x_uia_error, sizeof(g_x_uia_error), "%s: HRESULT 0x%08lx", op, (unsigned long)hr);
	xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.uia", (int)hr, g_x_uia_error);
}

static void x_uia_json_init(XUiaJsonBuilder* json)
{
	xrtClearError();
	json->data = g_x_uia_json;
	json->capacity = g_x_uia_json_capacity;
	json->length = 0u;
	json->overflow = 0;
	if ( json->data != NULL ) { json->data[0] = '\0'; }
}

static void x_uia_json_append_raw(XUiaJsonBuilder* json, const char* text)
{
	size_t len;
	size_t needed;
	size_t capacity;
	char* expanded;

	if ( json == NULL || text == NULL || json->overflow ) { return; }
	len = strlen(text);
	if ( len > X_UIA_JSON_MAX_BYTES - json->length ) {
		json->overflow = 1;
		return;
	}
	needed = json->length + len + 1u;
	if ( needed > json->capacity ) {
		capacity = json->capacity != 0u ? json->capacity : 1024u;
		while ( capacity < needed ) {
			capacity = capacity > (X_UIA_JSON_MAX_BYTES + 1u) / 2u ?
				X_UIA_JSON_MAX_BYTES + 1u : capacity * 2u;
		}
		expanded = json->data != NULL ?
			(char*)xrtRealloc(json->data, capacity) : (char*)xrtMalloc(capacity);
		if ( expanded == NULL ) { json->overflow = 2; return; }
		json->data = expanded;
		json->capacity = capacity;
		g_x_uia_json = expanded;
		g_x_uia_json_capacity = capacity;
	}
	memcpy(json->data + json->length, text, len);
	json->length += len;
	json->data[json->length] = '\0';
}

static const char* x_uia_json_finish(XUiaJsonBuilder* json, const char* limit_error)
{
	if ( json->overflow == 2 ) {
		x_uia_copy_text(g_x_uia_error, sizeof(g_x_uia_error),
			"UIA JSON allocation failed");
		if ( xrtGetError() == NULL ) {
			xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.uia", 0,
				"UIA JSON allocation failed");
		}
		return "{}";
	}
	if ( json->overflow ) {
		if ( xrtGetError() == NULL ) { x_uia_set_error(limit_error); }
		return "{}";
	}
	x_uia_set_error("");
	return json->data != NULL ? json->data : "{}";
}

static void x_uia_json_append_format(XUiaJsonBuilder* json, const char* fmt, ...)
{
	char temp[256];
	va_list args;
	int written;

	va_start(args, fmt);
	written = vsnprintf(temp, sizeof(temp), fmt, args);
	va_end(args);
	if ( written < 0 ) {
		if ( json != NULL ) {
			json->overflow = 1;
		}
		return;
	}
	if ( (size_t)written >= sizeof(temp) ) {
		if ( json != NULL ) { json->overflow = 1; }
		return;
	}
	x_uia_json_append_raw(json, temp);
}

static void x_uia_json_append_escaped(XUiaJsonBuilder* json, const char* text)
{
	const unsigned char* p;
	char temp[8];

	x_uia_json_append_raw(json, "\"");
	if ( text == NULL ) {
		x_uia_json_append_raw(json, "\"");
		return;
	}
	for ( p = (const unsigned char*)text; *p != 0u; ++p ) {
		if ( json->overflow ) { return; }
		switch ( *p ) {
			case '\"':
				x_uia_json_append_raw(json, "\\\"");
				break;
			case '\\':
				x_uia_json_append_raw(json, "\\\\");
				break;
			case '\b':
				x_uia_json_append_raw(json, "\\b");
				break;
			case '\f':
				x_uia_json_append_raw(json, "\\f");
				break;
			case '\n':
				x_uia_json_append_raw(json, "\\n");
				break;
			case '\r':
				x_uia_json_append_raw(json, "\\r");
				break;
			case '\t':
				x_uia_json_append_raw(json, "\\t");
				break;
			default:
				if ( *p < 0x20u ) {
					snprintf(temp, sizeof(temp), "\\u%04x", (unsigned int)*p);
					x_uia_json_append_raw(json, temp);
				} else {
					temp[0] = (char)*p;
					temp[1] = '\0';
					x_uia_json_append_raw(json, temp);
				}
				break;
		}
	}
	x_uia_json_append_raw(json, "\"");
}

static int x_uia_ensure(void)
{
	HRESULT hr;
	LONG current = (LONG)GetCurrentThreadId();
	LONG owner = InterlockedCompareExchange(&g_x_uia_owner_thread, current, 0);

	if ( owner != 0 && owner != current ) {
		x_uia_set_error("UIA context belongs to another thread");
		return 0;
	}

	if ( g_x_uia != NULL ) {
		return 1;
	}
	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if ( SUCCEEDED(hr) ) {
		g_x_uia_com_initialized = 1;
	} else if ( hr != RPC_E_CHANGED_MODE ) {
		x_uia_set_hresult_error("CoInitializeEx", hr);
		InterlockedExchange(&g_x_uia_owner_thread, 0);
		return 0;
	}
	hr = CoCreateInstance(&X_UIA_CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER, &X_UIA_IID_IUIAutomation, (void**)&g_x_uia);
	if ( FAILED(hr) || (g_x_uia == NULL) ) {
		x_uia_set_hresult_error("CoCreateInstance(CUIAutomation)", hr);
		if ( g_x_uia_com_initialized ) {
			CoUninitialize();
			g_x_uia_com_initialized = 0;
		}
		InterlockedExchange(&g_x_uia_owner_thread, 0);
		return 0;
	}
	x_uia_set_error("");
	return 1;
}

static XUiaElement* x_uia_wrap_element(IUIAutomationElement* element)
{
	XUiaElement* wrapped;

	if ( element == NULL ) {
		return NULL;
	}
	wrapped = (XUiaElement*)calloc(1u, sizeof(XUiaElement));
	if ( wrapped == NULL ) {
		element->lpVtbl->Release(element);
		x_uia_set_error("element: out of memory");
		return NULL;
	}
	wrapped->magic = X_UIA_MAGIC_ELEMENT;
	wrapped->element = element;
	InterlockedIncrement(&g_x_uia_live_handles);
	return wrapped;
}

static XUiaCondition* x_uia_wrap_condition(IUIAutomationCondition* condition)
{
	XUiaCondition* wrapped;

	if ( condition == NULL ) {
		return NULL;
	}
	wrapped = (XUiaCondition*)calloc(1u, sizeof(XUiaCondition));
	if ( wrapped == NULL ) {
		condition->lpVtbl->Release(condition);
		x_uia_set_error("condition: out of memory");
		return NULL;
	}
	wrapped->magic = X_UIA_MAGIC_CONDITION;
	wrapped->condition = condition;
	InterlockedIncrement(&g_x_uia_live_handles);
	return wrapped;
}

static XUiaElement* x_uia_element_from_handle(void* handle)
{
	XUiaElement* element = (XUiaElement*)handle;
	if ( InterlockedCompareExchange(&g_x_uia_owner_thread, 0, 0) != (LONG)GetCurrentThreadId() ) return NULL;
	if ( (element == NULL) || (element->magic != X_UIA_MAGIC_ELEMENT) || (element->element == NULL) ) {
		return NULL;
	}
	return element;
}

static XUiaCondition* x_uia_condition_from_handle(void* handle)
{
	XUiaCondition* condition = (XUiaCondition*)handle;
	if ( InterlockedCompareExchange(&g_x_uia_owner_thread, 0, 0) != (LONG)GetCurrentThreadId() ) return NULL;
	if ( (condition == NULL) || (condition->magic != X_UIA_MAGIC_CONDITION) || (condition->condition == NULL) ) {
		return NULL;
	}
	return condition;
}

static BSTR x_uia_utf8_to_bstr(const char* text);

/* UIA 原生条件持有组合依赖，调用方可以独立释放子条件句柄。 */
static XUiaCondition* x_uia_condition_create(int kind, const char* text_value, int int_value, const char* op)
{
	IUIAutomationCondition* native = NULL;
	XUiaCondition* wrapped;
	PROPERTYID property_id;
	VARIANT value;
	BSTR text = NULL;
	HRESULT hr;

	if ( !x_uia_ensure() ) {
		return NULL;
	}
	switch ( kind ) {
	case X_UIA_CONDITION_NAME: property_id = UIA_NamePropertyId; break;
	case X_UIA_CONDITION_AUTOMATION_ID: property_id = UIA_AutomationIdPropertyId; break;
	case X_UIA_CONDITION_CLASS_NAME: property_id = UIA_ClassNamePropertyId; break;
	case X_UIA_CONDITION_CONTROL_TYPE: property_id = UIA_ControlTypePropertyId; break;
	case X_UIA_CONDITION_PROCESS_ID: property_id = UIA_ProcessIdPropertyId; break;
	default:
		x_uia_set_error("condition: unsupported property");
		return NULL;
	}
	VariantInit(&value);
	if ( kind == X_UIA_CONDITION_CONTROL_TYPE || kind == X_UIA_CONDITION_PROCESS_ID ) {
		value.vt = VT_I4;
		value.lVal = int_value;
	} else {
		text = x_uia_utf8_to_bstr(text_value);
		if ( text == NULL ) {
			x_uia_set_error("condition: UTF-8 conversion failed");
			return NULL;
		}
		value.vt = VT_BSTR;
		value.bstrVal = text;
	}
#if defined(__TINYC__) && defined(_WIN64)
	hr = g_x_uia->lpVtbl->CreatePropertyCondition(g_x_uia, property_id, &value, &native);
#else
	hr = g_x_uia->lpVtbl->CreatePropertyCondition(g_x_uia, property_id, value, &native);
#endif
	SysFreeString(text);
	if ( FAILED(hr) || native == NULL ) {
		x_uia_set_hresult_error(op, hr);
		return NULL;
	}
	wrapped = x_uia_wrap_condition(native);
	if ( wrapped != NULL ) x_uia_set_error("");
	return wrapped;
}

static XUiaCondition* x_uia_condition_compose(int kind, XUiaCondition* left, XUiaCondition* right, const char* op)
{
	IUIAutomationCondition* native = NULL;
	XUiaCondition* wrapped;
	HRESULT hr;

	if ( left == NULL || (kind != X_UIA_CONDITION_NOT && right == NULL) ) {
		x_uia_set_error("condition: invalid child");
		return NULL;
	}
	if ( !x_uia_ensure() ) return NULL;
	if ( kind == X_UIA_CONDITION_AND ) {
		hr = g_x_uia->lpVtbl->CreateAndCondition(g_x_uia, left->condition, right->condition, &native);
	} else if ( kind == X_UIA_CONDITION_OR ) {
		hr = g_x_uia->lpVtbl->CreateOrCondition(g_x_uia, left->condition, right->condition, &native);
	} else {
		hr = g_x_uia->lpVtbl->CreateNotCondition(g_x_uia, left->condition, &native);
	}
	if ( FAILED(hr) || native == NULL ) {
		x_uia_set_hresult_error(op, hr);
		return NULL;
	}
	wrapped = x_uia_wrap_condition(native);
	if ( wrapped != NULL ) x_uia_set_error("");
	return wrapped;
}

static BSTR x_uia_utf8_to_bstr(const char* text)
{
	int wide_len;
	BSTR out;

	if ( text == NULL ) {
		text = "";
	}
	wide_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
	if ( wide_len <= 0 ) {
		return NULL;
	}
	out = SysAllocStringLen(NULL, (UINT)(wide_len - 1));
	if ( out == NULL ) {
		return NULL;
	}
	if ( MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, out, wide_len) != wide_len ) {
		SysFreeString(out);
		return NULL;
	}
	return out;
}

static const char* x_uia_bstr_to_text(BSTR value)
{
	UINT length;
	int bytes;
	char* expanded;
	if ( value == NULL ) { return ""; }
	length = SysStringLen(value);
	if ( length == 0u ) { return ""; }
	if ( length > INT_MAX || wmemchr(value, L'\0', length) != NULL ) {
		x_uia_set_error("string conversion: invalid BSTR length or embedded NUL");
		return NULL;
	}
	bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value,
		(int)length, NULL, 0, NULL, NULL);
	if ( bytes <= 0 ) {
		x_uia_set_error("string conversion: invalid UTF-16");
		return NULL;
	}
	if ( (size_t)bytes + 1u > g_x_uia_bstr_capacity ) {
		expanded = (char*)xrtRealloc(g_x_uia_bstr_text, (size_t)bytes + 1u);
		if ( expanded == NULL ) {
			x_uia_copy_text(g_x_uia_error, sizeof(g_x_uia_error),
				"string conversion: out of memory");
			if ( xrtGetError() == NULL ) {
				xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.uia", 0,
					"string conversion: out of memory");
			}
			return NULL;
		}
		g_x_uia_bstr_text = expanded;
		g_x_uia_bstr_capacity = (size_t)bytes + 1u;
	}
	if ( WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value,
		(int)length, g_x_uia_bstr_text, bytes, NULL, NULL) != bytes ) {
		x_uia_set_error("string conversion failed");
		return NULL;
	}
	g_x_uia_bstr_text[bytes] = '\0';
	return g_x_uia_bstr_text;
}

static void x_uia_json_append_bstr(XUiaJsonBuilder* json, BSTR value)
{
	const char* text = x_uia_bstr_to_text(value);
	if ( text == NULL ) {
		json->overflow = 1;
		return;
	}
	x_uia_json_append_escaped(json, text);
}

static int x_uia_bstr_to_buffer(BSTR value, char* buffer, size_t capacity)
{
	const char* text;
	size_t length;

	if ( buffer == NULL || capacity == 0u ) { return 0; }
	buffer[0] = '\0';
	text = x_uia_bstr_to_text(value);
	if ( text == NULL ) { return 0; }
	length = strlen(text);
	if ( length >= capacity ) {
		x_uia_set_error("locator: text exceeds field limit");
		return 0;
	}
	memcpy(buffer, text, length + 1u);
	return 1;
}

static int x_uia_current_bstr_to_buffer(
	IUIAutomationElement* element,
	HRESULT (STDMETHODCALLTYPE *getter)(IUIAutomationElement*, BSTR*),
	char* buffer,
	size_t capacity
)
{
	BSTR value = NULL;
	HRESULT hr;

	if ( (buffer == NULL) || (capacity == 0u) ) {
		return 0;
	}
	buffer[0] = '\0';
	if ( (element == NULL) || (getter == NULL) ) {
		return 1;
	}
	hr = getter(element, &value);
	if ( SUCCEEDED(hr) ) {
		int ok = x_uia_bstr_to_buffer(value, buffer, capacity);
		SysFreeString(value);
		return ok;
	}
	SysFreeString(value);
	x_uia_set_hresult_error("locator property", hr);
	return 0;
}

static const char* x_uia_element_bstr_property(void* handle, HRESULT (STDMETHODCALLTYPE *getter)(IUIAutomationElement*, BSTR*), const char* op)
{
	XUiaElement* wrapped;
	BSTR value = NULL;
	HRESULT hr;
	const char* text;

	wrapped = x_uia_element_from_handle(handle);
	if ( wrapped == NULL ) {
		x_uia_set_error("element property: invalid element");
		return "";
	}
	hr = getter(wrapped->element, &value);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error(op, hr);
		return "";
	}
	text = x_uia_bstr_to_text(value);
	SysFreeString(value);
	if ( text == NULL ) { return ""; }
	x_uia_set_error("");
	return text;
}

static void* x_uia_get_pattern(XUiaElement* wrapped, PATTERNID pattern_id, REFIID iid, const char* op)
{
	void* pattern = NULL;
	HRESULT hr;

	if ( wrapped == NULL ) {
		x_uia_set_error("pattern: invalid element");
		return NULL;
	}
	hr = wrapped->element->lpVtbl->GetCurrentPatternAs(wrapped->element, pattern_id, iid, &pattern);
	if ( FAILED(hr) || (pattern == NULL) ) {
		x_uia_set_hresult_error(op, hr);
		return NULL;
	}
	return pattern;
}

static int x_uia_guid_equal(REFIID left, const IID* right)
{
	int i;
	if ( (left == NULL) || (right == NULL) ) {
		return 0;
	}
	if ( (left->Data1 != right->Data1) || (left->Data2 != right->Data2) || (left->Data3 != right->Data3) ) {
		return 0;
	}
	for ( i = 0; i < 8; i++ ) {
		if ( left->Data4[i] != right->Data4[i] ) {
			return 0;
		}
	}
	return 1;
}

static XUiaEvents* x_uia_events_from_handle(void* handle)
{
	XUiaEvents* queue = (XUiaEvents*)handle;
	if ( InterlockedCompareExchange(&g_x_uia_owner_thread, 0, 0) != (LONG)GetCurrentThreadId() ) return NULL;
	if ( (queue == NULL) || (queue->magic != X_UIA_MAGIC_EVENTS) ) {
		return NULL;
	}
	return queue;
}

static void x_uia_event_push(XUiaEvents* queue, const char* type, int property_id, IUIAutomationElement* sender)
{
	XUiaEventRecord rec;
	UIA_HWND hwnd = NULL;
	CONTROLTYPEID control_type = 0;
	int slot;

	if ( (queue == NULL) || InterlockedCompareExchange(&queue->closing, 0, 0) != 0 ) {
		return;
	}
	memset(&rec, 0, sizeof(rec));
	x_uia_copy_text(rec.type, sizeof(rec.type), type);
	rec.property_id = property_id;
	if ( sender != NULL ) {
		sender->lpVtbl->get_CurrentNativeWindowHandle(sender, &hwnd);
		sender->lpVtbl->get_CurrentControlType(sender, &control_type);
	}
	rec.hwnd = hwnd;
	rec.control_type = (int)control_type;

	EnterCriticalSection(&queue->lock);
	if ( InterlockedCompareExchange(&queue->closing, 0, 0) != 0 ) {
		LeaveCriticalSection(&queue->lock);
		return;
	}
	if ( queue->count == X_UIA_EVENT_QUEUE_CAPACITY ) {
		queue->head = (queue->head + 1) % X_UIA_EVENT_QUEUE_CAPACITY;
		queue->count--;
		queue->overflow_count++;
	}
	slot = (queue->head + queue->count) % X_UIA_EVENT_QUEUE_CAPACITY;
	queue->records[slot] = rec;
	queue->count++;
	SetEvent(queue->signal);
	LeaveCriticalSection(&queue->lock);
}

static HRESULT STDMETHODCALLTYPE x_uia_focus_handler_query_interface(IUIAutomationFocusChangedEventHandler* self, REFIID riid, void** out)
{
	if ( out == NULL ) {
		return E_POINTER;
	}
	if ( x_uia_guid_equal(riid, &X_UIA_IID_IUnknown) || x_uia_guid_equal(riid, &X_UIA_IID_IUIAutomationFocusChangedEventHandler) ) {
		*out = self;
		x_uia_focus_handler_add_ref(self);
		return S_OK;
	}
	*out = NULL;
	return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE x_uia_focus_handler_add_ref(IUIAutomationFocusChangedEventHandler* self)
{
	XUiaFocusHandler* handler = (XUiaFocusHandler*)self;
	return (ULONG)InterlockedIncrement(&handler->ref_count);
}

static ULONG STDMETHODCALLTYPE x_uia_focus_handler_release(IUIAutomationFocusChangedEventHandler* self)
{
	XUiaFocusHandler* handler = (XUiaFocusHandler*)self;
	LONG count = InterlockedDecrement(&handler->ref_count);
	if ( count == 0 ) {
		x_uia_events_drop_ref(handler->queue);
		free(handler);
		return 0u;
	}
	return (ULONG)count;
}

static HRESULT STDMETHODCALLTYPE x_uia_focus_handler_handle(IUIAutomationFocusChangedEventHandler* self, IUIAutomationElement* sender)
{
	XUiaFocusHandler* handler = (XUiaFocusHandler*)self;
	x_uia_event_push(handler->queue, "focus", 0, sender);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE x_uia_property_handler_query_interface(IUIAutomationPropertyChangedEventHandler* self, REFIID riid, void** out)
{
	if ( out == NULL ) {
		return E_POINTER;
	}
	if ( x_uia_guid_equal(riid, &X_UIA_IID_IUnknown) || x_uia_guid_equal(riid, &X_UIA_IID_IUIAutomationPropertyChangedEventHandler) ) {
		*out = self;
		x_uia_property_handler_add_ref(self);
		return S_OK;
	}
	*out = NULL;
	return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE x_uia_property_handler_add_ref(IUIAutomationPropertyChangedEventHandler* self)
{
	XUiaPropertyHandler* handler = (XUiaPropertyHandler*)self;
	return (ULONG)InterlockedIncrement(&handler->ref_count);
}

static ULONG STDMETHODCALLTYPE x_uia_property_handler_release(IUIAutomationPropertyChangedEventHandler* self)
{
	XUiaPropertyHandler* handler = (XUiaPropertyHandler*)self;
	LONG count = InterlockedDecrement(&handler->ref_count);
	if ( count == 0 ) {
		x_uia_events_drop_ref(handler->queue);
		free(handler);
		return 0u;
	}
	return (ULONG)count;
}

static HRESULT STDMETHODCALLTYPE x_uia_property_handler_handle(IUIAutomationPropertyChangedEventHandler* self, IUIAutomationElement* sender, PROPERTYID property_id, VARIANT new_value)
{
	XUiaPropertyHandler* handler = (XUiaPropertyHandler*)self;
	(void)new_value;
	x_uia_event_push(handler->queue, "property", (int)property_id, sender);
	return S_OK;
}

XL_EXPORT const char* x_uia_version(void)
{
	return X_UIA_VERSION;
}

XL_EXPORT const char* x_uia_last_error(void)
{
	LONG owner = InterlockedCompareExchange(&g_x_uia_owner_thread, 0, 0);
	if ( owner != 0 && (DWORD)owner != GetCurrentThreadId() ) return "UIA context belongs to another thread";
	return g_x_uia_error;
}

XL_EXPORT int x_uia_shutdown(void)
{
	LONG owner = InterlockedCompareExchange(&g_x_uia_owner_thread, 0, 0);
	if ( owner != 0 && (DWORD)owner != GetCurrentThreadId() ) {
		x_uia_set_error("UIA context belongs to another thread");
		return 0;
	}
	if ( InterlockedCompareExchange(&g_x_uia_live_handles, 0, 0) != 0 ) {
		x_uia_set_error("shutdown: UIA handles are still active");
		return 0;
	}
	if ( g_x_uia != NULL ) {
		g_x_uia->lpVtbl->Release(g_x_uia);
		g_x_uia = NULL;
	}
	if ( g_x_uia_com_initialized ) {
		CoUninitialize();
		g_x_uia_com_initialized = 0;
	}
	xrtFree(g_x_uia_bstr_text);
	g_x_uia_bstr_text = NULL;
	g_x_uia_bstr_capacity = 0u;
	xrtFree(g_x_uia_json);
	g_x_uia_json = NULL;
	g_x_uia_json_capacity = 0u;
	InterlockedExchange(&g_x_uia_owner_thread, 0);
	x_uia_set_error("");
	return 1;
}

XL_EXPORT void* x_uia_launch(const char* command)
{
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	XUiaProcess* process;
	BSTR command_line;
	BOOL ok;

	command_line = x_uia_utf8_to_bstr(command);
	if ( command_line == NULL ) {
		x_uia_set_error("launch: string conversion failed");
		return NULL;
	}
	memset(&si, 0, sizeof(si));
	memset(&pi, 0, sizeof(pi));
	si.cb = sizeof(si);
	ok = CreateProcessW(NULL, command_line, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
	SysFreeString(command_line);
	if ( !ok ) {
		snprintf(g_x_uia_error, sizeof(g_x_uia_error), "launch: Win32 error %lu", (unsigned long)GetLastError());
		return NULL;
	}
	process = (XUiaProcess*)calloc(1u, sizeof(XUiaProcess));
	if ( process == NULL ) {
		TerminateProcess(pi.hProcess, 1u);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		x_uia_set_error("launch: out of memory");
		return NULL;
	}
	process->magic = X_UIA_MAGIC_PROCESS;
	process->process = pi.hProcess;
	process->thread = pi.hThread;
	process->pid = pi.dwProcessId;
	x_uia_set_error("");
	return process;
}

XL_EXPORT int x_uia_process_valid(void* handle)
{
	return x_uia_process_from_handle(handle) != NULL;
}

XL_EXPORT int x_uia_process_id(void* handle)
{
	XUiaProcess* process = x_uia_process_from_handle(handle);
	if ( process == NULL ) {
		x_uia_set_error("process_id: invalid process");
		return 0;
	}
	return (int)process->pid;
}

XL_EXPORT int x_uia_process_close(void* handle)
{
	XUiaProcess* process = x_uia_process_from_handle(handle);
	if ( process == NULL ) {
		return 0;
	}
	if ( process->process != NULL ) {
		TerminateProcess(process->process, 0u);
		WaitForSingleObject(process->process, 2000u);
	}
	x_uia_set_error("");
	return 1;
}

XL_EXPORT int x_uia_process_release(void* handle)
{
	XUiaProcess* process = x_uia_process_from_handle(handle);
	if ( process == NULL ) {
		return 0;
	}
	if ( process->thread != NULL ) {
		CloseHandle(process->thread);
		process->thread = NULL;
	}
	if ( process->process != NULL ) {
		CloseHandle(process->process);
		process->process = NULL;
	}
	process->magic = 0u;
	free(process);
	return 1;
}

XL_EXPORT void* x_uia_test_window_create(void)
{
	XUiaTestWindow* win = (XUiaTestWindow*)calloc(1u, sizeof(XUiaTestWindow));
	if ( win == NULL ) {
		x_uia_set_error("test_window: out of memory");
		return NULL;
	}
	win->magic = X_UIA_MAGIC_TEST_WINDOW;
	win->ready_event = CreateEventA(NULL, TRUE, FALSE, NULL);
	if ( win->ready_event == NULL ) {
		free(win);
		x_uia_set_error("test_window: CreateEvent failed");
		return NULL;
	}
	win->thread = CreateThread(NULL, 0, x_uia_test_window_thread, win, 0, &win->thread_id);
	if ( win->thread == NULL ) {
		CloseHandle(win->ready_event);
		free(win);
		x_uia_set_error("test_window: CreateThread failed");
		return NULL;
	}
	WaitForSingleObject(win->ready_event, 5000u);
	if ( (win->hwnd == NULL) || (win->edit_hwnd == NULL) || (win->button_hwnd == NULL) ) {
		x_uia_set_error("test_window: window creation failed");
		return win;
	}
	x_uia_set_error("");
	return win;
}

XL_EXPORT int x_uia_test_window_valid(void* handle)
{
	XUiaTestWindow* win = x_uia_test_window_from_handle(handle);
	return (win != NULL) && (win->hwnd != NULL) && IsWindow(win->hwnd);
}

XL_EXPORT void* x_uia_test_window_hwnd(void* handle)
{
	XUiaTestWindow* win = x_uia_test_window_from_handle(handle);
	if ( win == NULL ) {
		x_uia_set_error("test_window.hwnd: invalid window");
		return NULL;
	}
	return win->hwnd;
}

XL_EXPORT void* x_uia_test_window_edit_hwnd(void* handle)
{
	XUiaTestWindow* win = x_uia_test_window_from_handle(handle);
	if ( win == NULL ) {
		x_uia_set_error("test_window.edit_hwnd: invalid window");
		return NULL;
	}
	return win->edit_hwnd;
}

XL_EXPORT void* x_uia_test_window_button_hwnd(void* handle)
{
	XUiaTestWindow* win = x_uia_test_window_from_handle(handle);
	if ( win == NULL ) {
		x_uia_set_error("test_window.button_hwnd: invalid window");
		return NULL;
	}
	return win->button_hwnd;
}

XL_EXPORT void* x_uia_test_window_checkbox_hwnd(void* handle)
{
	XUiaTestWindow* win = x_uia_test_window_from_handle(handle);
	if ( win == NULL ) {
		x_uia_set_error("test_window.checkbox_hwnd: invalid window");
		return NULL;
	}
	return win->checkbox_hwnd;
}

XL_EXPORT int x_uia_test_window_focus_edit(void* handle)
{
	XUiaTestWindow* win = x_uia_test_window_from_handle(handle);
	if ( (win == NULL) || (win->hwnd == NULL) ) {
		x_uia_set_error("test_window.focus_edit: invalid window");
		return 0;
	}
	PostMessageA(win->hwnd, X_UIA_TEST_WINDOW_FOCUS_EDIT, 0u, 0);
	x_uia_set_error("");
	return 1;
}

XL_EXPORT int x_uia_test_window_focus_button(void* handle)
{
	XUiaTestWindow* win = x_uia_test_window_from_handle(handle);
	if ( (win == NULL) || (win->hwnd == NULL) ) {
		x_uia_set_error("test_window.focus_button: invalid window");
		return 0;
	}
	PostMessageA(win->hwnd, X_UIA_TEST_WINDOW_FOCUS_BUTTON, 0u, 0);
	x_uia_set_error("");
	return 1;
}

XL_EXPORT int x_uia_test_window_close(void* handle)
{
	XUiaTestWindow* win = x_uia_test_window_from_handle(handle);
	DWORD waited;
	if ( win == NULL ) {
		return 0;
	}
	if ( (win->hwnd != NULL) && IsWindow(win->hwnd) ) {
		PostMessageA(win->hwnd, WM_CLOSE, 0, 0);
	}
	if ( win->thread != NULL ) {
		waited = WaitForSingleObject(win->thread, 2000u);
		if ( waited != WAIT_OBJECT_0 ) {
			x_uia_set_error("test_window.close: window thread did not stop");
			return 0;
		}
	}
	x_uia_set_error("");
	return 1;
}

XL_EXPORT int x_uia_test_window_release(void* handle)
{
	XUiaTestWindow* win = x_uia_test_window_from_handle(handle);
	if ( win == NULL ) {
		return 0;
	}
	if ( !x_uia_test_window_close(handle) ) {
		return 0;
	}
	if ( win->thread != NULL ) {
		CloseHandle(win->thread);
		win->thread = NULL;
	}
	if ( win->ready_event != NULL ) {
		CloseHandle(win->ready_event);
		win->ready_event = NULL;
	}
	win->magic = 0u;
	free(win);
	return 1;
}

XL_EXPORT void* x_uia_events_create(void)
{
	XUiaEvents* queue;

	if ( !x_uia_ensure() ) {
		return NULL;
	}
	queue = (XUiaEvents*)calloc(1u, sizeof(XUiaEvents));
	if ( queue == NULL ) {
		x_uia_set_error("events_create: out of memory");
		return NULL;
	}
	queue->signal = CreateEventA(NULL, TRUE, FALSE, NULL);
	if ( queue->signal == NULL ) {
		free(queue);
		x_uia_set_error("events_create: CreateEvent failed");
		return NULL;
	}
	InitializeCriticalSection(&queue->lock);
	queue->magic = X_UIA_MAGIC_EVENTS;
	queue->ref_count = 1;
	InterlockedIncrement(&g_x_uia_live_handles);
	x_uia_set_error("");
	return queue;
}

XL_EXPORT int x_uia_events_valid(void* handle)
{
	return x_uia_events_from_handle(handle) != NULL;
}

XL_EXPORT int x_uia_events_subscribe_focus_changed(void* handle)
{
	XUiaEvents* queue = x_uia_events_from_handle(handle);
	XUiaFocusHandler* handler;
	HRESULT hr;

	if ( queue == NULL ) {
		x_uia_set_error("events.subscribe_focus_changed: invalid queue");
		return 0;
	}
	if ( queue->focus_subscribed ) {
		x_uia_set_error("");
		return 1;
	}
	if ( !x_uia_ensure() ) {
		return 0;
	}
	handler = (XUiaFocusHandler*)calloc(1u, sizeof(XUiaFocusHandler));
	if ( handler == NULL ) {
		x_uia_set_error("events.subscribe_focus_changed: out of memory");
		return 0;
	}
	handler->lpVtbl = (IUIAutomationFocusChangedEventHandlerVtbl*)&g_x_uia_focus_handler_vtbl;
	handler->ref_count = 1;
	handler->queue = queue;
	hr = g_x_uia->lpVtbl->AddFocusChangedEventHandler(g_x_uia, NULL, (IUIAutomationFocusChangedEventHandler*)handler);
	if ( FAILED(hr) ) {
		free(handler);
		x_uia_set_hresult_error("events.subscribe_focus_changed", hr);
		return 0;
	}
	InterlockedIncrement(&queue->ref_count);
	queue->focus_handler = handler;
	queue->focus_subscribed = 1;
	x_uia_set_error("");
	return 1;
}

XL_EXPORT int x_uia_events_subscribe_property_changed(void* handle, void* element_handle, int property_id)
{
	XUiaEvents* queue = x_uia_events_from_handle(handle);
	XUiaElement* element = x_uia_element_from_handle(element_handle);
	XUiaPropertyHandler* handler;
	PROPERTYID property_array[1];
	HRESULT hr;

	if ( queue == NULL ) {
		x_uia_set_error("events.subscribe_property_changed: invalid queue");
		return 0;
	}
	if ( element == NULL ) {
		x_uia_set_error("events.subscribe_property_changed: invalid element");
		return 0;
	}
	if ( queue->property_subscribed ) {
		x_uia_set_error("events.subscribe_property_changed: queue already has a property subscription");
		return 0;
	}
	if ( !x_uia_ensure() ) {
		return 0;
	}
	handler = (XUiaPropertyHandler*)calloc(1u, sizeof(XUiaPropertyHandler));
	if ( handler == NULL ) {
		x_uia_set_error("events.subscribe_property_changed: out of memory");
		return 0;
	}
	handler->lpVtbl = (IUIAutomationPropertyChangedEventHandlerVtbl*)&g_x_uia_property_handler_vtbl;
	handler->ref_count = 1;
	handler->queue = queue;
	property_array[0] = (PROPERTYID)property_id;
	hr = g_x_uia->lpVtbl->AddPropertyChangedEventHandlerNativeArray(
		g_x_uia,
		element->element,
		TreeScope_Element,
		NULL,
		(IUIAutomationPropertyChangedEventHandler*)handler,
		property_array,
		1);
	if ( FAILED(hr) ) {
		free(handler);
		x_uia_set_hresult_error("events.subscribe_property_changed", hr);
		return 0;
	}
	InterlockedIncrement(&queue->ref_count);
	element->element->lpVtbl->AddRef(element->element);
	queue->property_element = element->element;
	queue->property_handler = handler;
	queue->property_id = (PROPERTYID)property_id;
	queue->property_subscribed = 1;
	x_uia_set_error("");
	return 1;
}

XL_EXPORT const char* x_uia_events_poll_json(void* handle, int timeout_ms)
{
	XUiaEvents* queue = x_uia_events_from_handle(handle);
	XUiaEventRecord rec;
	XUiaJsonBuilder json;
	int has_event = 0;
	int overflow = 0;
	DWORD started;
	DWORD elapsed;
	DWORD wait_ms;
	DWORD wait_result;
	MSG msg;

	if ( queue == NULL ) {
		x_uia_set_error("events.poll_json: invalid queue");
		return "{}";
	}
	started = GetTickCount();
	for ( ;; ) {
		EnterCriticalSection(&queue->lock);
		has_event = queue->count > 0;
		LeaveCriticalSection(&queue->lock);
		if ( has_event ) {
			break;
		}
		if ( timeout_ms < 0 ) {
			wait_ms = INFINITE;
		} else {
			elapsed = GetTickCount() - started;
			if ( elapsed >= (DWORD)timeout_ms ) {
				x_uia_set_error("");
				return "{}";
			}
			wait_ms = (DWORD)timeout_ms - elapsed;
		}
		wait_result = MsgWaitForMultipleObjects(1u, &queue->signal, FALSE, wait_ms, QS_ALLINPUT);
		if ( wait_result == WAIT_OBJECT_0 ) {
			break;
		}
		if ( wait_result == WAIT_OBJECT_0 + 1u ) {
			while ( PeekMessageA(&msg, NULL, 0u, 0u, PM_REMOVE) ) {
				TranslateMessage(&msg);
				DispatchMessageA(&msg);
			}
			continue;
		}
		if ( wait_result == WAIT_TIMEOUT ) {
			x_uia_set_error("");
		} else {
			snprintf(g_x_uia_error, sizeof(g_x_uia_error), "events.poll_json: Win32 wait error %lu", (unsigned long)GetLastError());
			xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.uia", 1, g_x_uia_error);
		}
		return "{}";
	}
	EnterCriticalSection(&queue->lock);
	if ( queue->count > 0 ) {
		rec = queue->records[queue->head];
		queue->head = (queue->head + 1) % X_UIA_EVENT_QUEUE_CAPACITY;
		queue->count--;
		has_event = 1;
	}
	overflow = queue->overflow_count;
	queue->overflow_count = 0;
	if ( queue->count == 0 ) {
		ResetEvent(queue->signal);
	}
	LeaveCriticalSection(&queue->lock);
	if ( !has_event ) {
		x_uia_set_error("");
		return "{}";
	}
	x_uia_json_init(&json);
	x_uia_json_append_format(
		&json,
		"{\"type\":\"%s\",\"propertyId\":%d,\"controlType\":%d,\"hwnd\":\"%p\",\"overflow\":%d}",
		rec.type,
		rec.property_id,
		rec.control_type,
		rec.hwnd,
		overflow);
	return x_uia_json_finish(&json, "events.poll_json: output limit exceeded");
}

XL_EXPORT int x_uia_events_release(void* handle)
{
	XUiaEvents* queue = x_uia_events_from_handle(handle);
	HRESULT hr;

	if ( queue == NULL ) {
		return 0;
	}
	InterlockedExchange(&queue->closing, 1);
	if ( queue->property_subscribed ) {
		hr = g_x_uia->lpVtbl->RemovePropertyChangedEventHandler(
			g_x_uia, queue->property_element,
			(IUIAutomationPropertyChangedEventHandler*)queue->property_handler);
		if ( FAILED(hr) ) {
			x_uia_set_hresult_error("events.release property", hr);
			return 0;
		}
		queue->property_subscribed = 0;
		queue->property_element->lpVtbl->Release(queue->property_element);
		queue->property_element = NULL;
		queue->property_handler->lpVtbl->Release((IUIAutomationPropertyChangedEventHandler*)queue->property_handler);
		queue->property_handler = NULL;
	}
	if ( queue->focus_subscribed ) {
		hr = g_x_uia->lpVtbl->RemoveFocusChangedEventHandler(
			g_x_uia, (IUIAutomationFocusChangedEventHandler*)queue->focus_handler);
		if ( FAILED(hr) ) {
			x_uia_set_hresult_error("events.release focus", hr);
			return 0;
		}
		queue->focus_subscribed = 0;
		queue->focus_handler->lpVtbl->Release((IUIAutomationFocusChangedEventHandler*)queue->focus_handler);
		queue->focus_handler = NULL;
	}
	queue->magic = 0u;
	x_uia_events_drop_ref(queue);
	x_uia_set_error("");
	return 1;
}

XL_EXPORT void* x_uia_desktop(void)
{
	IUIAutomationElement* root = NULL;
	HRESULT hr;

	if ( !x_uia_ensure() ) {
		return NULL;
	}
	hr = g_x_uia->lpVtbl->GetRootElement(g_x_uia, &root);
	if ( FAILED(hr) || (root == NULL) ) {
		x_uia_set_hresult_error("desktop", hr);
		return NULL;
	}
	return x_uia_wrap_element(root);
}

XL_EXPORT void* x_uia_focused(void)
{
	IUIAutomationElement* focused = NULL;
	HRESULT hr;

	if ( !x_uia_ensure() ) {
		return NULL;
	}
	hr = g_x_uia->lpVtbl->GetFocusedElement(g_x_uia, &focused);
	if ( FAILED(hr) || (focused == NULL) ) {
		x_uia_set_hresult_error("focused", hr);
		return NULL;
	}
	return x_uia_wrap_element(focused);
}

XL_EXPORT void* x_uia_from_hwnd(void* hwnd)
{
	IUIAutomationElement* element = NULL;
	HRESULT hr;

	if ( !x_uia_ensure() ) {
		return NULL;
	}
	hr = g_x_uia->lpVtbl->ElementFromHandle(g_x_uia, hwnd, &element);
	if ( FAILED(hr) || (element == NULL) ) {
		x_uia_set_hresult_error("from_hwnd", hr);
		return NULL;
	}
	return x_uia_wrap_element(element);
}

XL_EXPORT void* x_uia_from_point(int x, int y)
{
	IUIAutomationElement* element = NULL;
	POINT pt;
	HRESULT hr;

	if ( !x_uia_ensure() ) {
		return NULL;
	}
	pt.x = x;
	pt.y = y;
	hr = g_x_uia->lpVtbl->ElementFromPoint(g_x_uia, pt, &element);
	if ( FAILED(hr) || (element == NULL) ) {
		x_uia_set_hresult_error("from_point", hr);
		return NULL;
	}
	return x_uia_wrap_element(element);
}

XL_EXPORT int x_uia_element_release(void* handle)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	if ( wrapped == NULL ) {
		return 0;
	}
	if ( wrapped->element != NULL ) {
		wrapped->element->lpVtbl->Release(wrapped->element);
		wrapped->element = NULL;
	}
	wrapped->magic = 0u;
	free(wrapped);
	InterlockedDecrement(&g_x_uia_live_handles);
	return 1;
}

XL_EXPORT int x_uia_element_valid(void* handle)
{
	return x_uia_element_from_handle(handle) != NULL;
}

XL_EXPORT const char* x_uia_element_name(void* handle)
{
	return x_uia_element_bstr_property(handle, x_uia_element_from_handle(handle) ? x_uia_element_from_handle(handle)->element->lpVtbl->get_CurrentName : NULL, "name");
}

XL_EXPORT const char* x_uia_element_automation_id(void* handle)
{
	return x_uia_element_bstr_property(handle, x_uia_element_from_handle(handle) ? x_uia_element_from_handle(handle)->element->lpVtbl->get_CurrentAutomationId : NULL, "automation_id");
}

XL_EXPORT const char* x_uia_element_class_name(void* handle)
{
	return x_uia_element_bstr_property(handle, x_uia_element_from_handle(handle) ? x_uia_element_from_handle(handle)->element->lpVtbl->get_CurrentClassName : NULL, "class_name");
}

XL_EXPORT int x_uia_element_control_type(void* handle)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	CONTROLTYPEID control_type = 0;
	HRESULT hr;

	if ( wrapped == NULL ) {
		x_uia_set_error("control_type: invalid element");
		return 0;
	}
	hr = wrapped->element->lpVtbl->get_CurrentControlType(wrapped->element, &control_type);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error("control_type", hr);
		return 0;
	}
	return control_type;
}

XL_EXPORT void* x_uia_element_hwnd(void* handle)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	UIA_HWND hwnd = NULL;
	HRESULT hr;

	if ( wrapped == NULL ) {
		x_uia_set_error("hwnd: invalid element");
		return NULL;
	}
	hr = wrapped->element->lpVtbl->get_CurrentNativeWindowHandle(wrapped->element, &hwnd);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error("hwnd", hr);
		return NULL;
	}
	return hwnd;
}

XL_EXPORT const char* x_uia_element_rect_json(void* handle)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	RECT rect;
	HRESULT hr;

	if ( wrapped == NULL ) {
		x_uia_set_error("rect: invalid element");
		return "{}";
	}
	memset(&rect, 0, sizeof(rect));
	hr = wrapped->element->lpVtbl->get_CurrentBoundingRectangle(wrapped->element, &rect);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error("rect", hr);
		return "{}";
	}
	snprintf(
		g_x_uia_text,
		sizeof(g_x_uia_text),
		"{\"left\":%ld,\"top\":%ld,\"right\":%ld,\"bottom\":%ld,\"width\":%ld,\"height\":%ld}",
		(long)rect.left,
		(long)rect.top,
		(long)rect.right,
		(long)rect.bottom,
		(long)(rect.right - rect.left),
		(long)(rect.bottom - rect.top)
	);
	return g_x_uia_text;
}

static void x_uia_snapshot_append_bstr_property(
	XUiaSnapshotContext* ctx,
	IUIAutomationElement* element,
	const char* name,
	HRESULT (STDMETHODCALLTYPE *getter)(IUIAutomationElement*, BSTR*)
)
{
	BSTR value = NULL;
	HRESULT hr;

	x_uia_json_append_raw(&ctx->json, ",\"");
	x_uia_json_append_raw(&ctx->json, name);
	x_uia_json_append_raw(&ctx->json, "\":");
	hr = getter(element, &value);
	if ( FAILED(hr) ) {
		x_uia_json_append_escaped(&ctx->json, "");
		SysFreeString(value);
		return;
	}
	x_uia_json_append_bstr(&ctx->json, value);
	SysFreeString(value);
}

static void x_uia_snapshot_append_element(XUiaSnapshotContext* ctx, IUIAutomationElement* element, int depth)
{
	CONTROLTYPEID control_type = 0;
	UIA_HWND hwnd = NULL;
	RECT rect;
	IUIAutomationElement* child = NULL;
	HRESULT hr;
	int first;

	if ( (ctx == NULL) || (element == NULL) ) {
		return;
	}
	if ( ctx->node_count >= ctx->max_nodes ) {
		ctx->truncated = 1;
		x_uia_json_append_raw(&ctx->json, "{\"truncated\":true}");
		return;
	}
	++ctx->node_count;
	memset(&rect, 0, sizeof(rect));
	(void)element->lpVtbl->get_CurrentControlType(element, &control_type);
	(void)element->lpVtbl->get_CurrentNativeWindowHandle(element, &hwnd);
	(void)element->lpVtbl->get_CurrentBoundingRectangle(element, &rect);
	x_uia_json_append_raw(&ctx->json, "{");
	x_uia_json_append_raw(&ctx->json, "\"name\":");
	{
		BSTR value = NULL;
		hr = element->lpVtbl->get_CurrentName(element, &value);
		if ( SUCCEEDED(hr) ) { x_uia_json_append_bstr(&ctx->json, value); }
		else { x_uia_json_append_escaped(&ctx->json, ""); }
		SysFreeString(value);
	}
	x_uia_snapshot_append_bstr_property(ctx, element, "automationId", element->lpVtbl->get_CurrentAutomationId);
	x_uia_snapshot_append_bstr_property(ctx, element, "className", element->lpVtbl->get_CurrentClassName);
	x_uia_json_append_format(&ctx->json, ",\"controlType\":%d", (int)control_type);
	x_uia_json_append_raw(&ctx->json, ",\"hwnd\":");
	x_uia_json_append_format(&ctx->json, "\"%p\"", hwnd);
	x_uia_json_append_format(
		&ctx->json,
		",\"rect\":{\"left\":%ld,\"top\":%ld,\"right\":%ld,\"bottom\":%ld,\"width\":%ld,\"height\":%ld}",
		(long)rect.left,
		(long)rect.top,
		(long)rect.right,
		(long)rect.bottom,
		(long)(rect.right - rect.left),
		(long)(rect.bottom - rect.top)
	);
	x_uia_json_append_raw(&ctx->json, ",\"children\":[");
	first = 1;
	if ( depth > 0 ) {
		hr = ctx->walker->lpVtbl->GetFirstChildElement(ctx->walker, element, &child);
		while ( SUCCEEDED(hr) && (child != NULL) ) {
			IUIAutomationElement* next = NULL;
			if ( ctx->node_count >= ctx->max_nodes ) {
				ctx->truncated = 1;
				child->lpVtbl->Release(child);
				break;
			}
			if ( !first ) {
				x_uia_json_append_raw(&ctx->json, ",");
			}
			first = 0;
			x_uia_snapshot_append_element(ctx, child, depth - 1);
			hr = ctx->walker->lpVtbl->GetNextSiblingElement(ctx->walker, child, &next);
			child->lpVtbl->Release(child);
			child = next;
		}
	}
	x_uia_json_append_raw(&ctx->json, "]");
	if ( ctx->truncated || ctx->json.overflow ) {
		x_uia_json_append_raw(&ctx->json, ",\"truncated\":true");
	}
	x_uia_json_append_raw(&ctx->json, "}");
}

XL_EXPORT const char* x_uia_element_snapshot_json(void* handle, int depth, int max_nodes)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	XUiaSnapshotContext ctx;
	HRESULT hr;

	if ( !x_uia_ensure() ) {
		return "{}";
	}
	if ( wrapped == NULL ) {
		x_uia_set_error("snapshot_json: invalid element");
		return "{}";
	}
	if ( depth < 0 ) {
		depth = 0;
	}
	if ( max_nodes <= 0 ) {
		max_nodes = 64;
	}
	if ( max_nodes > 512 ) {
		max_nodes = 512;
	}
	memset(&ctx, 0, sizeof(ctx));
	x_uia_json_init(&ctx.json);
	ctx.max_nodes = max_nodes;
	hr = g_x_uia->lpVtbl->get_ControlViewWalker(g_x_uia, &ctx.walker);
	if ( FAILED(hr) || (ctx.walker == NULL) ) {
		x_uia_set_hresult_error("snapshot_json walker", hr);
		return "{}";
	}
	x_uia_snapshot_append_element(&ctx, wrapped->element, depth);
	ctx.walker->lpVtbl->Release(ctx.walker);
	return x_uia_json_finish(&ctx.json, "snapshot_json: output limit exceeded");
}

static HRESULT x_uia_create_snapshot_cache_request(IUIAutomationCacheRequest** request)
{
	IUIAutomationCacheRequest* cache = NULL;
	HRESULT hr;

	if ( request == NULL ) {
		return E_POINTER;
	}
	*request = NULL;
	hr = g_x_uia->lpVtbl->CreateCacheRequest(g_x_uia, &cache);
	if ( FAILED(hr) || (cache == NULL) ) {
		return hr;
	}
	(void)cache->lpVtbl->put_TreeScope(cache, TreeScope_Subtree);
	(void)cache->lpVtbl->put_AutomationElementMode(cache, AutomationElementMode_Full);
	(void)cache->lpVtbl->AddProperty(cache, UIA_NamePropertyId);
	(void)cache->lpVtbl->AddProperty(cache, UIA_AutomationIdPropertyId);
	(void)cache->lpVtbl->AddProperty(cache, UIA_ClassNamePropertyId);
	(void)cache->lpVtbl->AddProperty(cache, UIA_ControlTypePropertyId);
	(void)cache->lpVtbl->AddProperty(cache, UIA_NativeWindowHandlePropertyId);
	(void)cache->lpVtbl->AddProperty(cache, UIA_BoundingRectanglePropertyId);
	*request = cache;
	return S_OK;
}

static void x_uia_cached_snapshot_append_element(XUiaSnapshotContext* ctx, IUIAutomationElement* element, int depth)
{
	CONTROLTYPEID control_type = 0;
	UIA_HWND hwnd = NULL;
	RECT rect;
	IUIAutomationElementArray* children = NULL;
	HRESULT hr;
	int length = 0;
	int i;

	if ( (ctx == NULL) || (element == NULL) ) {
		return;
	}
	if ( ctx->node_count >= ctx->max_nodes ) {
		ctx->truncated = 1;
		x_uia_json_append_raw(&ctx->json, "{\"truncated\":true}");
		return;
	}
	++ctx->node_count;
	memset(&rect, 0, sizeof(rect));
	(void)element->lpVtbl->get_CachedControlType(element, &control_type);
	(void)element->lpVtbl->get_CachedNativeWindowHandle(element, &hwnd);
	(void)element->lpVtbl->get_CachedBoundingRectangle(element, &rect);
	x_uia_json_append_raw(&ctx->json, "{");
	x_uia_json_append_raw(&ctx->json, "\"name\":");
	{
		BSTR value = NULL;
		hr = element->lpVtbl->get_CachedName(element, &value);
		if ( SUCCEEDED(hr) ) { x_uia_json_append_bstr(&ctx->json, value); }
		else { x_uia_json_append_escaped(&ctx->json, ""); }
		SysFreeString(value);
	}
	x_uia_snapshot_append_bstr_property(ctx, element, "automationId", element->lpVtbl->get_CachedAutomationId);
	x_uia_snapshot_append_bstr_property(ctx, element, "className", element->lpVtbl->get_CachedClassName);
	x_uia_json_append_format(&ctx->json, ",\"controlType\":%d", (int)control_type);
	x_uia_json_append_raw(&ctx->json, ",\"hwnd\":");
	x_uia_json_append_format(&ctx->json, "\"%p\"", hwnd);
	x_uia_json_append_format(
		&ctx->json,
		",\"rect\":{\"left\":%ld,\"top\":%ld,\"right\":%ld,\"bottom\":%ld,\"width\":%ld,\"height\":%ld}",
		(long)rect.left,
		(long)rect.top,
		(long)rect.right,
		(long)rect.bottom,
		(long)(rect.right - rect.left),
		(long)(rect.bottom - rect.top)
	);
	x_uia_json_append_raw(&ctx->json, ",\"children\":[");
	if ( depth > 0 ) {
		hr = element->lpVtbl->GetCachedChildren(element, &children);
		if ( SUCCEEDED(hr) && (children != NULL) ) {
			(void)children->lpVtbl->get_Length(children, &length);
			for ( i = 0; i < length; i++ ) {
				IUIAutomationElement* child = NULL;
				if ( ctx->node_count >= ctx->max_nodes ) {
					ctx->truncated = 1;
					break;
				}
				hr = children->lpVtbl->GetElement(children, i, &child);
				if ( FAILED(hr) || (child == NULL) ) {
					continue;
				}
				if ( i > 0 ) {
					x_uia_json_append_raw(&ctx->json, ",");
				}
				x_uia_cached_snapshot_append_element(ctx, child, depth - 1);
				child->lpVtbl->Release(child);
			}
			children->lpVtbl->Release(children);
		}
	}
	x_uia_json_append_raw(&ctx->json, "]");
	if ( ctx->truncated || ctx->json.overflow ) {
		x_uia_json_append_raw(&ctx->json, ",\"truncated\":true");
	}
	x_uia_json_append_raw(&ctx->json, "}");
}

XL_EXPORT const char* x_uia_element_cached_children_json(void* handle, int depth, int max_nodes)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	IUIAutomationCacheRequest* cache = NULL;
	IUIAutomationElement* cached = NULL;
	XUiaSnapshotContext ctx;
	HRESULT hr;

	if ( !x_uia_ensure() ) {
		return "{}";
	}
	if ( wrapped == NULL ) {
		x_uia_set_error("cached_children_json: invalid element");
		return "{}";
	}
	if ( depth < 0 ) {
		depth = 0;
	}
	if ( max_nodes <= 0 ) {
		max_nodes = 64;
	}
	if ( max_nodes > 512 ) {
		max_nodes = 512;
	}
	hr = x_uia_create_snapshot_cache_request(&cache);
	if ( FAILED(hr) || (cache == NULL) ) {
		x_uia_set_hresult_error("cached_children_json cache request", hr);
		return "{}";
	}
	hr = wrapped->element->lpVtbl->BuildUpdatedCache(wrapped->element, cache, &cached);
	cache->lpVtbl->Release(cache);
	if ( FAILED(hr) || (cached == NULL) ) {
		x_uia_set_hresult_error("cached_children_json build cache", hr);
		return "{}";
	}
	memset(&ctx, 0, sizeof(ctx));
	x_uia_json_init(&ctx.json);
	ctx.max_nodes = max_nodes;
	x_uia_cached_snapshot_append_element(&ctx, cached, depth);
	cached->lpVtbl->Release(cached);
	return x_uia_json_finish(&ctx.json, "cached_children_json: output limit exceeded");
}

static int x_uia_locator_same_element(IUIAutomationElement* left, IUIAutomationElement* right)
{
	WINBOOL same = FALSE;
	HRESULT hr;

	if ( (left == NULL) || (right == NULL) ) {
		return 0;
	}
	if ( g_x_uia != NULL ) {
		hr = g_x_uia->lpVtbl->CompareElements(g_x_uia, left, right, &same);
		if ( SUCCEEDED(hr) ) {
			return same ? 1 : 0;
		}
	}
	return left == right;
}

static int x_uia_locator_same_signature(IUIAutomationElement* element, const char* class_name, int control_type)
{
	char child_class[512];
	CONTROLTYPEID child_type = 0;
	HRESULT hr;

	if ( element == NULL ) {
		return 0;
	}
	if ( !x_uia_current_bstr_to_buffer(element, element->lpVtbl->get_CurrentClassName,
		child_class, sizeof(child_class)) ) { return -1; }
	hr = element->lpVtbl->get_CurrentControlType(element, &child_type);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error("locator child control type", hr);
		return -1;
	}
	return (child_type == control_type) && (strcmp(child_class, class_name) == 0);
}

static int x_uia_locator_sibling_index(
	IUIAutomationTreeWalker* walker,
	IUIAutomationElement* parent,
	IUIAutomationElement* target,
	const char* class_name,
	int control_type
)
{
	IUIAutomationElement* child = NULL;
	HRESULT hr;
	int index = 0;

	if ( (walker == NULL) || (parent == NULL) || (target == NULL) ) {
		return 0;
	}
	hr = walker->lpVtbl->GetFirstChildElement(walker, parent, &child);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error("locator first child", hr);
		return -1;
	}
	while ( SUCCEEDED(hr) && (child != NULL) ) {
		IUIAutomationElement* next = NULL;
		int same_signature = x_uia_locator_same_signature(child, class_name, control_type);
		if ( same_signature < 0 ) {
			child->lpVtbl->Release(child);
			return -1;
		}
		if ( x_uia_locator_same_element(child, target) ) {
			child->lpVtbl->Release(child);
			return index;
		}
		if ( same_signature ) {
			++index;
		}
		hr = walker->lpVtbl->GetNextSiblingElement(walker, child, &next);
		child->lpVtbl->Release(child);
		child = next;
	}
	if ( child != NULL ) { child->lpVtbl->Release(child); }
	if ( FAILED(hr) ) { x_uia_set_hresult_error("locator next sibling", hr); }
	else { x_uia_set_error("locator: target is not a child of its parent"); }
	return -1;
}

static int x_uia_locator_segment(char* output, size_t capacity,
	const char* class_name, int control_type, int index)
{
	static const char hex[] = "0123456789ABCDEF";
	char suffix[32];
	int suffix_length;
	size_t used = 0u;
	const unsigned char* cursor = (const unsigned char*)class_name;
	while ( *cursor != 0u ) {
		int escape = *cursor == '/' || *cursor == '[' || *cursor == ']' ||
			*cursor == '%' || *cursor == '#';
		size_t width = escape ? 3u : 1u;
		if ( used + width >= capacity ) { return 0; }
		if ( escape ) {
			output[used++] = '%';
			output[used++] = hex[*cursor >> 4];
			output[used++] = hex[*cursor & 15u];
		} else {
			output[used++] = (char)*cursor;
		}
		++cursor;
	}
	suffix_length = snprintf(suffix, sizeof(suffix), "#%d[%d]", control_type, index);
	if ( suffix_length < 0 || suffix_length >= (int)sizeof(suffix) ||
		used + (size_t)suffix_length >= capacity ) { return 0; }
	memcpy(output + used, suffix, (size_t)suffix_length + 1u);
	return 1;
}

static int x_uia_locator_build_path(IUIAutomationElement* element, XUiaLocatorInfo* info)
{
	IUIAutomationTreeWalker* walker = NULL;
	IUIAutomationElement* current;
	HRESULT hr;
	char segments[16][160];
	int segment_count = 0;
	int i;

	if ( (element == NULL) || (info == NULL) ) {
		return 0;
	}
	info->class_path[0] = '\0';
	hr = g_x_uia->lpVtbl->get_ControlViewWalker(g_x_uia, &walker);
	if ( FAILED(hr) || (walker == NULL) ) {
		x_uia_set_hresult_error("locator walker", hr);
		return 0;
	}
	current = element;
	current->lpVtbl->AddRef(current);
	while ( (current != NULL) && (segment_count < (int)(sizeof(segments) / sizeof(segments[0]))) ) {
		IUIAutomationElement* parent = NULL;
		char class_name[512];
		CONTROLTYPEID control_type = 0;
		int sibling_index = 0;

		if ( !x_uia_current_bstr_to_buffer(current, current->lpVtbl->get_CurrentClassName,
			class_name, sizeof(class_name)) ) {
			current->lpVtbl->Release(current);
			walker->lpVtbl->Release(walker);
			return 0;
		}
		hr = current->lpVtbl->get_CurrentControlType(current, &control_type);
		if ( FAILED(hr) ) {
			current->lpVtbl->Release(current);
			walker->lpVtbl->Release(walker);
			x_uia_set_hresult_error("locator path control type", hr);
			return 0;
		}
		hr = walker->lpVtbl->GetParentElement(walker, current, &parent);
		if ( FAILED(hr) ) {
			if ( parent != NULL ) { parent->lpVtbl->Release(parent); }
			current->lpVtbl->Release(current);
			walker->lpVtbl->Release(walker);
			x_uia_set_hresult_error("locator parent", hr);
			return 0;
		}
		if ( SUCCEEDED(hr) && (parent != NULL) ) {
			sibling_index = x_uia_locator_sibling_index(walker, parent, current, class_name, (int)control_type);
			if ( sibling_index < 0 ) {
				parent->lpVtbl->Release(parent);
				current->lpVtbl->Release(current);
				walker->lpVtbl->Release(walker);
				return 0;
			}
		}
		if ( segment_count == 0 ) {
			info->sibling_index = sibling_index;
		}
		if ( class_name[0] != '\0' ) {
			if ( !x_uia_locator_segment(segments[segment_count], sizeof(segments[segment_count]),
				class_name, (int)control_type, sibling_index) ) {
				x_uia_set_error("locator: class path segment exceeds limit");
				if ( parent != NULL ) { parent->lpVtbl->Release(parent); }
				current->lpVtbl->Release(current);
				walker->lpVtbl->Release(walker);
				return 0;
			}
		} else {
			(void)snprintf(segments[segment_count], sizeof(segments[segment_count]), "controlType:%d[%d]", (int)control_type, sibling_index);
		}
		++segment_count;
		current->lpVtbl->Release(current);
		current = parent;
	}
	if ( current != NULL ) {
		current->lpVtbl->Release(current);
		walker->lpVtbl->Release(walker);
		x_uia_set_error("locator: class path exceeds depth limit");
		return 0;
	}
	for ( i = segment_count - 1; i >= 0; --i ) {
		size_t used = strlen(info->class_path);
		size_t needed = used + (used > 0u ? 1u : 0u) + strlen(segments[i]);
		if ( needed >= sizeof(info->class_path) ) {
			walker->lpVtbl->Release(walker);
			x_uia_set_error("locator: class path exceeds text limit");
			return 0;
		}
		if ( used > 0u ) {
			x_uia_append_text(info->class_path, sizeof(info->class_path), "/");
		}
		x_uia_append_text(info->class_path, sizeof(info->class_path), segments[i]);
	}
	walker->lpVtbl->Release(walker);
	return 1;
}

static int x_uia_locator_read_info(IUIAutomationElement* element, XUiaLocatorInfo* info)
{
	HRESULT hr;
	if ( (element == NULL) || (info == NULL) ) {
		return 0;
	}
	memset(info, 0, sizeof(*info));
	if ( !x_uia_current_bstr_to_buffer(element, element->lpVtbl->get_CurrentName,
		info->name, sizeof(info->name)) ||
		!x_uia_current_bstr_to_buffer(element, element->lpVtbl->get_CurrentAutomationId,
			info->automation_id, sizeof(info->automation_id)) ||
		!x_uia_current_bstr_to_buffer(element, element->lpVtbl->get_CurrentClassName,
			info->class_name, sizeof(info->class_name)) ) { return 0; }
	hr = element->lpVtbl->get_CurrentControlType(element, &info->control_type);
	if ( FAILED(hr) ) { x_uia_set_hresult_error("locator control type", hr); return 0; }
	hr = element->lpVtbl->get_CurrentProcessId(element, &info->process_id);
	if ( FAILED(hr) ) { x_uia_set_hresult_error("locator process id", hr); return 0; }
	hr = element->lpVtbl->get_CurrentNativeWindowHandle(element, &info->hwnd);
	if ( FAILED(hr) ) { x_uia_set_hresult_error("locator window handle", hr); return 0; }
	hr = element->lpVtbl->get_CurrentBoundingRectangle(element, &info->rect);
	if ( FAILED(hr) ) { x_uia_set_hresult_error("locator rectangle", hr); return 0; }
	return x_uia_locator_build_path(element, info);
}

static const char* x_uia_locator_json_for_element(IUIAutomationElement* element)
{
	XUiaLocatorInfo info;
	XUiaJsonBuilder json;
	const char* strategy;
	const char* expression;
	int confidence;

	if ( element == NULL ) {
		x_uia_set_error("locator: invalid element");
		return "{}";
	}
	x_uia_json_init(&json);
	if ( !x_uia_locator_read_info(element, &info) ) { return "{}"; }
	if ( info.automation_id[0] != '\0' ) {
		strategy = "automationId";
		expression = "by_automation_id + by_control_type";
		confidence = 3;
	} else if ( info.class_name[0] != '\0' ) {
		strategy = "classNameName";
		expression = "by_class_name + optional by_name + by_control_type";
		confidence = 2;
	} else {
		strategy = "processClassPath";
		expression = "by_process_id + class_path + index";
		confidence = 1;
	}
	if ( info.class_path[0] == '\0' ) {
		(void)snprintf(info.class_path, sizeof(info.class_path), "%s[0]", info.class_name[0] != '\0' ? info.class_name : "element");
	}
	x_uia_json_append_raw(&json, "{");
	x_uia_json_append_raw(&json, "\"strategy\":");
	x_uia_json_append_escaped(&json, strategy);
	x_uia_json_append_format(&json, ",\"confidence\":%d", confidence);
	x_uia_json_append_raw(&json, ",\"expression\":");
	x_uia_json_append_escaped(&json, expression);
	x_uia_json_append_raw(&json, ",\"automationId\":");
	x_uia_json_append_escaped(&json, info.automation_id);
	x_uia_json_append_raw(&json, ",\"className\":");
	x_uia_json_append_escaped(&json, info.class_name);
	x_uia_json_append_raw(&json, ",\"name\":");
	x_uia_json_append_escaped(&json, info.name);
	x_uia_json_append_format(&json, ",\"controlType\":%d", info.control_type);
	x_uia_json_append_format(&json, ",\"processId\":%d", info.process_id);
	x_uia_json_append_raw(&json, ",\"hwnd\":");
	x_uia_json_append_format(&json, "\"%p\"", info.hwnd);
	x_uia_json_append_raw(&json, ",\"classPath\":");
	x_uia_json_append_escaped(&json, info.class_path);
	x_uia_json_append_format(&json, ",\"index\":%d", info.sibling_index);
	x_uia_json_append_format(
		&json,
		",\"rect\":{\"left\":%ld,\"top\":%ld,\"right\":%ld,\"bottom\":%ld,\"width\":%ld,\"height\":%ld}",
		(long)info.rect.left,
		(long)info.rect.top,
		(long)info.rect.right,
		(long)info.rect.bottom,
		(long)(info.rect.right - info.rect.left),
		(long)(info.rect.bottom - info.rect.top)
	);
	x_uia_json_append_raw(&json, "}");
	return x_uia_json_finish(&json, "locator: output limit exceeded");
}

XL_EXPORT const char* x_uia_locator_for(void* handle)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);

	if ( !x_uia_ensure() ) {
		return "{}";
	}
	if ( wrapped == NULL ) {
		x_uia_set_error("locator_for: invalid element");
		return "{}";
	}
	return x_uia_locator_json_for_element(wrapped->element);
}

XL_EXPORT const char* x_uia_focused_locator(void)
{
	IUIAutomationElement* focused = NULL;
	HRESULT hr;
	const char* json;

	if ( !x_uia_ensure() ) {
		return "{}";
	}
	hr = g_x_uia->lpVtbl->GetFocusedElement(g_x_uia, &focused);
	if ( FAILED(hr) || (focused == NULL) ) {
		x_uia_set_hresult_error("focused_locator", hr);
		return "{}";
	}
	json = x_uia_locator_json_for_element(focused);
	focused->lpVtbl->Release(focused);
	return json;
}

static int x_uia_locator_path_prefix(const char* target, const char* prefix)
{
	size_t length = strlen(prefix);
	return strncmp(target, prefix, length) == 0 &&
		(target[length] == '\0' || target[length] == '/');
}

static int x_uia_locator_process_matches(IUIAutomationElement* element, int process_id)
{
	int actual = 0;
	HRESULT hr;
	if ( process_id == 0 ) { return 1; }
	hr = element->lpVtbl->get_CurrentProcessId(element, &actual);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error("class path process id", hr);
		return -1;
	}
	return actual == process_id;
}

static IUIAutomationElement* x_uia_locator_follow_path(
	IUIAutomationTreeWalker* walker,
	IUIAutomationElement* parent,
	const char* target,
	int process_id,
	int* failed
)
{
	IUIAutomationElement* child = NULL;
	HRESULT hr = walker->lpVtbl->GetFirstChildElement(walker, parent, &child);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error("class path first child", hr);
		*failed = 1;
		return NULL;
	}
	while ( child != NULL ) {
		IUIAutomationElement* next = NULL;
		IUIAutomationElement* match = NULL;
		XUiaLocatorInfo info;
		memset(&info, 0, sizeof(info));
		if ( !x_uia_locator_build_path(child, &info) ) {
			*failed = 1;
		} else if ( x_uia_locator_path_prefix(target, info.class_path) ) {
			if ( strcmp(target, info.class_path) == 0 ) {
				int process_match = x_uia_locator_process_matches(child, process_id);
				if ( process_match < 0 ) { *failed = 1; }
				else if ( process_match ) { child->lpVtbl->AddRef(child); match = child; }
			} else {
				match = x_uia_locator_follow_path(walker, child, target, process_id, failed);
			}
		}
		if ( match != NULL || *failed ) {
			child->lpVtbl->Release(child);
			return match;
		}
		hr = walker->lpVtbl->GetNextSiblingElement(walker, child, &next);
		child->lpVtbl->Release(child);
		if ( FAILED(hr) ) {
			if ( next != NULL ) { next->lpVtbl->Release(next); }
			x_uia_set_hresult_error("class path next sibling", hr);
			*failed = 1;
			return NULL;
		}
		child = next;
	}
	return NULL;
}

XL_EXPORT void* x_uia_element_find_class_path(void* root, const char* class_path, int process_id, int timeout_ms)
{
	XUiaElement* wrapped = x_uia_element_from_handle(root);
	DWORD started = GetTickCount();
	XUiaLocatorInfo root_info;
	if ( wrapped == NULL || class_path == NULL || class_path[0] == '\0' ||
		strlen(class_path) >= sizeof(root_info.class_path) || process_id < 0 || timeout_ms < 0 ) {
		x_uia_set_error("find_class_path: invalid root, path or numeric filter");
		return NULL;
	}
	memset(&root_info, 0, sizeof(root_info));
	if ( !x_uia_locator_build_path(wrapped->element, &root_info) ) { return NULL; }
	if ( !x_uia_locator_path_prefix(class_path, root_info.class_path) ) {
		x_uia_set_error("");
		return NULL;
	}
	for ( ;; ) {
		IUIAutomationElement* match = NULL;
		int failed = 0;
		if ( strcmp(class_path, root_info.class_path) == 0 ) {
			int process_match = x_uia_locator_process_matches(wrapped->element, process_id);
			if ( process_match < 0 ) { return NULL; }
			if ( process_match ) {
				wrapped->element->lpVtbl->AddRef(wrapped->element);
				match = wrapped->element;
			}
		} else {
			IUIAutomationTreeWalker* walker = NULL;
			HRESULT hr = g_x_uia->lpVtbl->get_ControlViewWalker(g_x_uia, &walker);
			if ( FAILED(hr) || walker == NULL ) {
				x_uia_set_hresult_error("find_class_path walker", hr);
				return NULL;
			}
			match = x_uia_locator_follow_path(walker, wrapped->element, class_path,
				process_id, &failed);
			walker->lpVtbl->Release(walker);
		}
		if ( failed ) { return NULL; }
		if ( match != NULL ) {
			x_uia_set_error("");
			return x_uia_wrap_element(match);
		}
		if ( timeout_ms == 0 || (DWORD)(GetTickCount() - started) >= (DWORD)timeout_ms ) {
			x_uia_set_error("");
			return NULL;
		}
		Sleep(50);
	}
}

XL_EXPORT void* x_uia_by_name(const char* name)
{
	return x_uia_condition_create(X_UIA_CONDITION_NAME, name, 0, "by_name");
}

XL_EXPORT void* x_uia_by_automation_id(const char* automation_id)
{
	return x_uia_condition_create(X_UIA_CONDITION_AUTOMATION_ID, automation_id, 0, "by_automation_id");
}

XL_EXPORT void* x_uia_by_class_name(const char* class_name)
{
	return x_uia_condition_create(X_UIA_CONDITION_CLASS_NAME, class_name, 0, "by_class_name");
}

XL_EXPORT void* x_uia_by_control_type(int control_type)
{
	return x_uia_condition_create(X_UIA_CONDITION_CONTROL_TYPE, "", control_type, "by_control_type");
}

XL_EXPORT void* x_uia_by_process_id(int process_id)
{
	return x_uia_condition_create(X_UIA_CONDITION_PROCESS_ID, "", process_id, "by_process_id");
}

XL_EXPORT void* x_uia_condition_and(void* left, void* right)
{
	XUiaCondition* a = x_uia_condition_from_handle(left);
	XUiaCondition* b = x_uia_condition_from_handle(right);

	if ( (a == NULL) || (b == NULL) ) {
		x_uia_set_error("all_of: invalid condition");
		return NULL;
	}
	return x_uia_condition_compose(X_UIA_CONDITION_AND, a, b, "all_of");
}

XL_EXPORT void* x_uia_condition_or(void* left, void* right)
{
	XUiaCondition* a = x_uia_condition_from_handle(left);
	XUiaCondition* b = x_uia_condition_from_handle(right);

	if ( (a == NULL) || (b == NULL) ) {
		x_uia_set_error("any_of: invalid condition");
		return NULL;
	}
	return x_uia_condition_compose(X_UIA_CONDITION_OR, a, b, "any_of");
}

XL_EXPORT void* x_uia_condition_not(void* handle)
{
	XUiaCondition* wrapped = x_uia_condition_from_handle(handle);

	if ( wrapped == NULL ) {
		x_uia_set_error("not_condition: invalid condition");
		return NULL;
	}
	return x_uia_condition_compose(X_UIA_CONDITION_NOT, wrapped, NULL, "not_condition");
}

XL_EXPORT int x_uia_condition_release(void* handle)
{
	XUiaCondition* wrapped = x_uia_condition_from_handle(handle);
	if ( wrapped == NULL ) {
		return 0;
	}
	if ( wrapped->condition != NULL ) {
		wrapped->condition->lpVtbl->Release(wrapped->condition);
		wrapped->condition = NULL;
	}
	wrapped->magic = 0u;
	free(wrapped);
	InterlockedDecrement(&g_x_uia_live_handles);
	return 1;
}

XL_EXPORT int x_uia_condition_valid(void* handle)
{
	return x_uia_condition_from_handle(handle) != NULL;
}

XL_EXPORT void* x_uia_element_find_first(void* element_handle, void* condition_handle, int scope, int timeout_ms)
{
	XUiaElement* element = x_uia_element_from_handle(element_handle);
	XUiaCondition* condition = x_uia_condition_from_handle(condition_handle);
	IUIAutomationElement* item = NULL;
	DWORD started;
	DWORD now;
	HRESULT hr;
	int effective_timeout;

	if ( element == NULL ) {
		x_uia_set_error("find_first: invalid element");
		return NULL;
	}
	if ( condition == NULL ) {
		x_uia_set_error("find_first: invalid condition");
		return NULL;
	}
	effective_timeout = timeout_ms < 0 ? 0 : timeout_ms;
	started = GetTickCount();
	do {
		item = NULL;
		hr = element->element->lpVtbl->FindFirst(element->element, (enum TreeScope)scope, condition->condition, &item);
		if ( FAILED(hr) ) {
			x_uia_set_hresult_error("find_first", hr);
			return NULL;
		}
		if ( item != NULL ) {
			x_uia_set_error("");
			return x_uia_wrap_element(item);
		}
		now = GetTickCount();
		if ( effective_timeout <= 0 || (int)(now - started) >= effective_timeout ) {
			break;
		}
		Sleep(50);
	} while ( 1 );
	snprintf(g_x_uia_error, sizeof(g_x_uia_error), "find_first: timeout after %d ms", effective_timeout);
	return NULL;
}

XL_EXPORT xvalue* x_uia_element_find_all(void* element_handle, void* condition_handle, int scope, int timeout_ms)
{
	XUiaElement* element = x_uia_element_from_handle(element_handle);
	XUiaCondition* condition = x_uia_condition_from_handle(condition_handle);
	xvalue* out = xrtValueArray();
	IUIAutomationElementArray* found = NULL;
	IUIAutomationElement* item = NULL;
	HRESULT hr;
	DWORD started;
	DWORD elapsed;
	int count = 0;
	int i;

	if ( (element == NULL) || (condition == NULL) ) {
		x_uia_set_error("find_all: invalid element or condition");
		return out;
	}
	started = GetTickCount();
	for ( ;; ) {
		found = NULL;
		hr = element->element->lpVtbl->FindAll(element->element, (enum TreeScope)scope, condition->condition, &found);
		if ( FAILED(hr) || found == NULL ) {
			x_uia_set_hresult_error("find_all", hr);
			return out;
		}
		hr = found->lpVtbl->get_Length(found, &count);
		if ( FAILED(hr) ) {
			found->lpVtbl->Release(found);
			x_uia_set_hresult_error("find_all length", hr);
			return out;
		}
		if ( count > 0 || timeout_ms <= 0 ) break;
		elapsed = GetTickCount() - started;
		if ( elapsed >= (DWORD)timeout_ms ) break;
		found->lpVtbl->Release(found);
		Sleep((DWORD)timeout_ms - elapsed < 50u ? (DWORD)timeout_ms - elapsed : 50u);
	}
	for ( i = 0; i < count; ++i ) {
		item = NULL;
		if ( SUCCEEDED(found->lpVtbl->GetElement(found, i, &item)) && item != NULL ) {
			XUiaElement* wrapped = x_uia_wrap_element(item);
			if ( wrapped == NULL || !xrtValueArrayAppendNew(out, xrtValuePointer(wrapped)) ) {
				x_uia_element_release(wrapped);
			}
		}
	}
	found->lpVtbl->Release(found);
	x_uia_set_error("");
	return out;
}

static xvalue* x_uia_element_array_to_xvalue(IUIAutomationElementArray* found)
{
	xvalue* out = xrtValueArray();
	IUIAutomationElement* item = NULL;
	HRESULT hr;
	int count = 0;
	int i;

	if ( found == NULL ) {
		return out;
	}
	hr = found->lpVtbl->get_Length(found, &count);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error("element array length", hr);
		return out;
	}
	for ( i = 0; i < count; ++i ) {
		item = NULL;
		if ( SUCCEEDED(found->lpVtbl->GetElement(found, i, &item)) && (item != NULL) ) {
		{
			XUiaElement* wrapped = x_uia_wrap_element(item);
			if ( wrapped == NULL || !xrtValueArrayAppendNew(out,
				xrtValuePointer(wrapped)) ) {
				x_uia_element_release(wrapped);
			}
		}
		}
	}
	return out;
}

XL_EXPORT void* x_uia_element_parent(void* handle)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	IUIAutomationTreeWalker* walker = NULL;
	IUIAutomationElement* parent = NULL;
	HRESULT hr;

	if ( !x_uia_ensure() ) {
		return NULL;
	}
	if ( wrapped == NULL ) {
		x_uia_set_error("parent: invalid element");
		return NULL;
	}
	hr = g_x_uia->lpVtbl->get_RawViewWalker(g_x_uia, &walker);
	if ( FAILED(hr) || (walker == NULL) ) {
		x_uia_set_hresult_error("parent walker", hr);
		return NULL;
	}
	hr = walker->lpVtbl->GetParentElement(walker, wrapped->element, &parent);
	walker->lpVtbl->Release(walker);
	if ( FAILED(hr) || (parent == NULL) ) {
		x_uia_set_hresult_error("parent", hr);
		return NULL;
	}
	return x_uia_wrap_element(parent);
}

XL_EXPORT xvalue* x_uia_element_children(void* handle)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	xvalue* out = xrtValueArray();
	IUIAutomationCondition* true_condition = NULL;
	IUIAutomationElementArray* found = NULL;
	HRESULT hr;

	if ( !x_uia_ensure() ) {
		return out;
	}
	if ( wrapped == NULL ) {
		x_uia_set_error("children: invalid element");
		return out;
	}
	hr = g_x_uia->lpVtbl->CreateTrueCondition(g_x_uia, &true_condition);
	if ( FAILED(hr) || (true_condition == NULL) ) {
		x_uia_set_hresult_error("children true condition", hr);
		return out;
	}
	hr = wrapped->element->lpVtbl->FindAll(wrapped->element, TreeScope_Children, true_condition, &found);
	true_condition->lpVtbl->Release(true_condition);
	if ( FAILED(hr) || (found == NULL) ) {
		x_uia_set_hresult_error("children", hr);
		return out;
	}
	out = x_uia_element_array_to_xvalue(found);
	found->lpVtbl->Release(found);
	return out;
}

XL_EXPORT int x_uia_element_set_focus(void* handle)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	HRESULT hr;

	if ( wrapped == NULL ) {
		x_uia_set_error("set_focus: invalid element");
		return 0;
	}
	hr = wrapped->element->lpVtbl->SetFocus(wrapped->element);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error("set_focus", hr);
		return 0;
	}
	x_uia_set_error("");
	return 1;
}

XL_EXPORT int x_uia_element_invoke(void* handle)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	IUIAutomationInvokePattern* pattern;
	HRESULT hr;

	pattern = (IUIAutomationInvokePattern*)x_uia_get_pattern(wrapped, UIA_InvokePatternId, &X_UIA_IID_IUIAutomationInvokePattern, "invoke");
	if ( pattern == NULL ) {
		return 0;
	}
	hr = pattern->lpVtbl->Invoke(pattern);
	pattern->lpVtbl->Release(pattern);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error("invoke", hr);
		return 0;
	}
	x_uia_set_error("");
	return 1;
}

XL_EXPORT int x_uia_element_click(void* handle, int timeout_ms)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	RECT rect;
	int x;
	int y;

	(void)timeout_ms;
	if ( wrapped == NULL ) {
		x_uia_set_error("click: invalid element");
		return 0;
	}
	if ( x_uia_element_invoke(handle) ) {
		return 1;
	}
	memset(&rect, 0, sizeof(rect));
	if ( FAILED(wrapped->element->lpVtbl->get_CurrentBoundingRectangle(wrapped->element, &rect)) ||
		(rect.right <= rect.left) || (rect.bottom <= rect.top) ) {
		x_uia_set_error("click: no usable pattern and empty bounding rectangle");
		return 0;
	}
	wrapped->element->lpVtbl->SetFocus(wrapped->element);
	x = rect.left + ((rect.right - rect.left) / 2);
	y = rect.top + ((rect.bottom - rect.top) / 2);
	SetCursorPos(x, y);
	mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
	mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
	x_uia_set_error("");
	return 1;
}

XL_EXPORT const char* x_uia_element_value(void* handle)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	IUIAutomationValuePattern* pattern;
	BSTR value = NULL;
	HRESULT hr;
	const char* text;

	pattern = (IUIAutomationValuePattern*)x_uia_get_pattern(wrapped, UIA_ValuePatternId, &X_UIA_IID_IUIAutomationValuePattern, "value");
	if ( pattern == NULL ) {
		return "";
	}
	hr = pattern->lpVtbl->get_CurrentValue(pattern, &value);
	pattern->lpVtbl->Release(pattern);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error("value", hr);
		return "";
	}
	text = x_uia_bstr_to_text(value);
	SysFreeString(value);
	if ( text == NULL ) { return ""; }
	x_uia_set_error("");
	return text;
}

XL_EXPORT int x_uia_element_set_value(void* handle, const char* value)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	IUIAutomationValuePattern* pattern;
	BSTR bstr;
	HRESULT hr;

	pattern = (IUIAutomationValuePattern*)x_uia_get_pattern(wrapped, UIA_ValuePatternId, &X_UIA_IID_IUIAutomationValuePattern, "set_value");
	if ( pattern == NULL ) {
		return 0;
	}
	bstr = x_uia_utf8_to_bstr(value);
	if ( bstr == NULL ) {
		pattern->lpVtbl->Release(pattern);
		x_uia_set_error("set_value: string conversion failed");
		return 0;
	}
	hr = pattern->lpVtbl->SetValue(pattern, bstr);
	SysFreeString(bstr);
	pattern->lpVtbl->Release(pattern);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error("set_value", hr);
		return 0;
	}
	x_uia_set_error("");
	return 1;
}

XL_EXPORT const char* x_uia_element_text(void* handle)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	IUIAutomationTextPattern* pattern;
	IUIAutomationTextRange* range = NULL;
	BSTR text_bstr = NULL;
	HRESULT hr;
	const char* text;
	const char* value;

	pattern = (IUIAutomationTextPattern*)x_uia_get_pattern(wrapped, UIA_TextPatternId, &X_UIA_IID_IUIAutomationTextPattern, "text");
	if ( pattern != NULL ) {
		hr = pattern->lpVtbl->get_DocumentRange(pattern, &range);
		pattern->lpVtbl->Release(pattern);
		if ( SUCCEEDED(hr) && (range != NULL) ) {
			hr = range->lpVtbl->GetText(range, -1, &text_bstr);
			range->lpVtbl->Release(range);
			if ( SUCCEEDED(hr) ) {
				text = x_uia_bstr_to_text(text_bstr);
				SysFreeString(text_bstr);
				if ( text == NULL ) { return ""; }
				if ( text[0] != '\0' ) {
					x_uia_set_error("");
					return text;
				}
			} else {
				x_uia_set_hresult_error("text range", hr);
			}
		} else {
			x_uia_set_hresult_error("text document range", hr);
		}
	}

	value = x_uia_element_value(handle);
	if ( value[0] != '\0' ) {
		return value;
	}
	return x_uia_element_name(handle);
}

XL_EXPORT int x_uia_element_toggle(void* handle)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	IUIAutomationTogglePattern* pattern;
	HRESULT hr;

	pattern = (IUIAutomationTogglePattern*)x_uia_get_pattern(wrapped, UIA_TogglePatternId, &X_UIA_IID_IUIAutomationTogglePattern, "toggle");
	if ( pattern == NULL ) {
		return 0;
	}
	hr = pattern->lpVtbl->Toggle(pattern);
	pattern->lpVtbl->Release(pattern);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error("toggle", hr);
		return 0;
	}
	x_uia_set_error("");
	return 1;
}

XL_EXPORT int x_uia_element_toggle_state(void* handle)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	IUIAutomationTogglePattern* pattern;
	enum ToggleState state = ToggleState_Off;
	HRESULT hr;

	pattern = (IUIAutomationTogglePattern*)x_uia_get_pattern(wrapped, UIA_TogglePatternId, &X_UIA_IID_IUIAutomationTogglePattern, "toggle_state");
	if ( pattern == NULL ) {
		return -1;
	}
	hr = pattern->lpVtbl->get_CurrentToggleState(pattern, &state);
	pattern->lpVtbl->Release(pattern);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error("toggle_state", hr);
		return -1;
	}
	x_uia_set_error("");
	return (int)state;
}

static double x_uia_element_range_metric(void* handle, int metric, const char* op)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	IUIAutomationRangeValuePattern* pattern;
	double value = 0.0;
	HRESULT hr;

	pattern = (IUIAutomationRangeValuePattern*)x_uia_get_pattern(wrapped, UIA_RangeValuePatternId, &X_UIA_IID_IUIAutomationRangeValuePattern, op);
	if ( pattern == NULL ) {
		return 0.0;
	}
	if ( metric == 1 ) {
		hr = pattern->lpVtbl->get_CurrentMinimum(pattern, &value);
	} else if ( metric == 2 ) {
		hr = pattern->lpVtbl->get_CurrentMaximum(pattern, &value);
	} else {
		hr = pattern->lpVtbl->get_CurrentValue(pattern, &value);
	}
	pattern->lpVtbl->Release(pattern);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error(op, hr);
		return 0.0;
	}
	x_uia_set_error("");
	return value;
}

XL_EXPORT double x_uia_element_range_value(void* handle)
{
	return x_uia_element_range_metric(handle, 0, "range_value");
}

XL_EXPORT double x_uia_element_range_min(void* handle)
{
	return x_uia_element_range_metric(handle, 1, "range_min");
}

XL_EXPORT double x_uia_element_range_max(void* handle)
{
	return x_uia_element_range_metric(handle, 2, "range_max");
}

XL_EXPORT int x_uia_element_set_range_value(void* handle, double value)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	IUIAutomationRangeValuePattern* pattern;
	HRESULT hr;

	pattern = (IUIAutomationRangeValuePattern*)x_uia_get_pattern(wrapped, UIA_RangeValuePatternId, &X_UIA_IID_IUIAutomationRangeValuePattern, "set_range_value");
	if ( pattern == NULL ) {
		return 0;
	}
	hr = pattern->lpVtbl->SetValue(pattern, value);
	pattern->lpVtbl->Release(pattern);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error("set_range_value", hr);
		return 0;
	}
	x_uia_set_error("");
	return 1;
}

XL_EXPORT int x_uia_element_scroll(void* handle, int horizontal, int vertical)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	IUIAutomationScrollPattern* pattern;
	HRESULT hr;

	pattern = (IUIAutomationScrollPattern*)x_uia_get_pattern(wrapped, UIA_ScrollPatternId, &X_UIA_IID_IUIAutomationScrollPattern, "scroll");
	if ( pattern == NULL ) {
		return 0;
	}
	hr = pattern->lpVtbl->Scroll(pattern, (enum ScrollAmount)horizontal, (enum ScrollAmount)vertical);
	pattern->lpVtbl->Release(pattern);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error("scroll", hr);
		return 0;
	}
	x_uia_set_error("");
	return 1;
}

XL_EXPORT int x_uia_element_close_window(void* handle)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	IUIAutomationWindowPattern* pattern;
	HRESULT hr;

	pattern = (IUIAutomationWindowPattern*)x_uia_get_pattern(wrapped, UIA_WindowPatternId, &X_UIA_IID_IUIAutomationWindowPattern, "close_window");
	if ( pattern == NULL ) {
		return 0;
	}
	hr = pattern->lpVtbl->Close(pattern);
	pattern->lpVtbl->Release(pattern);
	if ( FAILED(hr) ) {
		x_uia_set_hresult_error("close_window", hr);
		return 0;
	}
	x_uia_set_error("");
	return 1;
}

XL_EXPORT int x_uia_element_wait_window_closed(void* handle, int timeout_ms)
{
	XUiaElement* wrapped = x_uia_element_from_handle(handle);
	UIA_HWND hwnd = NULL;
	DWORD started;
	DWORD now;
	HRESULT hr;
	int effective_timeout;

	if ( wrapped == NULL ) {
		x_uia_set_error("wait_window_closed: invalid element");
		return 0;
	}
	hr = wrapped->element->lpVtbl->get_CurrentNativeWindowHandle(wrapped->element, &hwnd);
	if ( FAILED(hr) || (hwnd == NULL) ) {
		x_uia_set_hresult_error("wait_window_closed hwnd", hr);
		return 0;
	}
	effective_timeout = timeout_ms > 0 ? timeout_ms : 5000;
	started = GetTickCount();
	while ( IsWindow((HWND)hwnd) ) {
		now = GetTickCount();
		if ( (DWORD)(now - started) >= (DWORD)effective_timeout ) {
			x_uia_set_error("wait_window_closed: timeout");
			return 0;
		}
		Sleep(25u);
	}
	x_uia_set_error("");
	return 1;
}
