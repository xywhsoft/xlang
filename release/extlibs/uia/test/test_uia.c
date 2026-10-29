/* Windows UI Automation 的动态值边界：元素集合必须返回当前 XRT xvalue 数组。 */
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/uia.h"
#include "../x_uia_min.h"
#include <stdio.h>
#include <stdlib.h>

/* 只在测试中模拟 OLE 分配失败，不改变产品的系统分配器。 */
static int g_x_uia_test_fail_bstr;
static int g_x_uia_test_fail_calloc;
static BSTR WINAPI x_uia_test_alloc_bstr(const OLECHAR* text, UINT length)
{
	return g_x_uia_test_fail_bstr ? NULL : SysAllocStringLen(text, length);
}
static void* x_uia_test_calloc(size_t count, size_t size)
{
	if ( g_x_uia_test_fail_calloc ) { g_x_uia_test_fail_calloc = 0; return NULL; }
	return calloc(count, size);
}
#define SysAllocStringLen x_uia_test_alloc_bstr
#define calloc x_uia_test_calloc
#include "../x_uia_artifact.c"
#undef SysAllocStringLen
#undef calloc

typedef struct XUiaThreadCheck {
	void* root;
	void* events;
	int valid;
	int shutdown;
	int release_events;
	void* desktop;
	void* process;
} XUiaThreadCheck;

static DWORD WINAPI x_uia_check_other_thread(void* context)
{
	XUiaThreadCheck* check = (XUiaThreadCheck*)context;
	check->valid = x_uia_element_valid(check->root);
	check->shutdown = x_uia_shutdown();
	check->release_events = x_uia_events_release(check->events);
	check->desktop = x_uia_desktop();
	check->process = x_uia_launch("");
	return 0;
}

static HRESULT STDMETHODCALLTYPE x_uia_test_long_name(IUIAutomationElement* element, BSTR* value)
{
	(void)element;
	*value = SysAllocStringLen(NULL, 9000u);
	if ( *value == NULL ) { return E_OUTOFMEMORY; }
	for ( size_t i = 0u; i < 9000u; ++i ) { (*value)[i] = L'x'; }
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE x_uia_test_failed_name(IUIAutomationElement* element, BSTR* value)
{
	(void)element;
	*value = NULL;
	return E_FAIL;
}

static void* g_x_uia_poll_release_queue;
static int g_x_uia_poll_release_result;

static VOID CALLBACK x_uia_test_release_during_poll(HWND hwnd, UINT message, UINT_PTR timer, DWORD time)
{
	(void)hwnd;
	(void)message;
	(void)time;
	KillTimer(NULL, timer);
	g_x_uia_poll_release_result = x_uia_events_release(g_x_uia_poll_release_queue);
	if ( g_x_uia_poll_release_result ) { g_x_uia_poll_release_queue = NULL; }
}

typedef struct XUiaMemoryProvider {
	IUIAutomationElement element;
	unsigned stage;
	unsigned pattern_calls;
	unsigned name_calls;
	unsigned releases;
} XUiaMemoryProvider;

static ULONG STDMETHODCALLTYPE x_uia_memory_provider_release(IUIAutomationElement* self)
{
	((XUiaMemoryProvider*)self)->releases++;
	return 0;
}

static HRESULT STDMETHODCALLTYPE x_uia_memory_provider_pattern(IUIAutomationElement* self,
	PATTERNID pattern, REFIID iid, void** result)
{
	XUiaMemoryProvider* provider = (XUiaMemoryProvider*)self;
	(void)iid;
	provider->pattern_calls++;
	*result = NULL;
	if ( provider->stage == 0u && pattern == UIA_TextPatternId ) { return E_OUTOFMEMORY; }
	if ( provider->stage == 1u && pattern == UIA_ValuePatternId ) {
		return HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY);
	}
	return E_NOINTERFACE;
}

static HRESULT STDMETHODCALLTYPE x_uia_memory_provider_name(IUIAutomationElement* self, BSTR* result)
{
	((XUiaMemoryProvider*)self)->name_calls++;
	*result = SysAllocStringLen(L"fallback", 8u);
	return *result != NULL ? S_OK : E_OUTOFMEMORY;
}

/* HRESULT OOM 不得触发后续回退；普通协议缺失仍可使用 Name。 */
static int x_uia_test_memory_fallback(void)
{
	static const IUIAutomationElementVtbl table = {
		.Release = x_uia_memory_provider_release,
		.GetCurrentPatternAs = x_uia_memory_provider_pattern,
		.get_CurrentName = x_uia_memory_provider_name
	};
	XUiaMemoryProvider provider = {0};
	void* handle;
	int ok = 1;
	provider.element.lpVtbl = &table;
	handle = x_uia_wrap_element(&provider.element);
	if ( handle == NULL ) { return 0; }
	for ( unsigned stage = 0u; stage < 3u; ++stage ) {
		char* result;
		provider.stage = stage;
		provider.pattern_calls = provider.name_calls = 0u;
		xrtClearError();
		result = x_uia_element_text_owned(handle);
		if ( stage < 2u ) {
			ok = result == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY &&
				provider.name_calls == 0u && provider.pattern_calls == stage + 1u;
		} else {
			ok = result != NULL && strcmp(result, "fallback") == 0 && xrtGetError() == NULL &&
				provider.name_calls == 1u && provider.pattern_calls == 2u;
		}
		xrtFree(result);
		if ( !ok ) { fprintf(stderr, "UIA text memory fallback failed: stage=%u\n", stage); break; }
	}
	if ( !x_uia_element_release(handle) || provider.releases != 1u ) { ok = 0; }
	return ok;
}

static int x_uia_test_bstr_allocation(void* element)
{
	if ( !x_uia_element_set_value(element, "owned text") ) { return 0; }
	for ( unsigned operation = 0u; operation < 3u; ++operation ) {
		void* handle = NULL;
		int result = 0;
		xrtClearError();
		g_x_uia_test_fail_bstr = 1;
		if ( operation == 0u ) { handle = x_uia_by_name("unused condition"); }
		if ( operation == 1u ) { handle = x_uia_launch("__uia_never_started_oom__"); }
		if ( operation == 2u ) { result = x_uia_element_set_value(element, "unapplied"); }
		g_x_uia_test_fail_bstr = 0;
		if ( handle != NULL || result != 0 || xrtErrorKind(xrtGetError()) != XERR_MEMORY ) {
			if ( handle != NULL && operation == 0u ) { x_uia_condition_release(handle); }
			if ( handle != NULL && operation == 1u ) { x_uia_process_release(handle); }
			fprintf(stderr, "UIA BSTR allocation failed: operation=%u\n", operation);
			return 0;
		}
	}
	xrtSetErrorInfo(XERR_MEMORY, "test", 0, "old failure");
	if ( x_uia_by_name("\xc3\x28") != NULL || xrtErrorKind(xrtGetError()) != XERR_VALUE ) { return 0; }
	return strcmp(x_uia_element_value(element), "owned text") == 0 && xrtGetError() == NULL;
}

/* 七处不透明资源包装的 CRT 分配失败也必须报告 Memory，而非 Value。 */
static int x_uia_test_handle_allocation(void* window, void* element)
{
	for ( unsigned operation = 0u; operation < 7u; ++operation ) {
		void* handle = NULL;
		void* events = operation >= 5u ? x_uia_events_create() : NULL;
		int result = 0;
		bool ok;
		if ( operation >= 5u && events == NULL ) { return 0; }
		xrtClearError();
		g_x_uia_test_fail_calloc = 1;
		switch ( operation ) {
		case 0u: handle = x_uia_from_hwnd(x_uia_test_window_hwnd(window)); break;
		case 1u: handle = x_uia_by_name("unused condition"); break;
		case 2u: handle = x_uia_test_window_create(); break;
		case 3u: handle = x_uia_events_create(); break;
		case 4u: handle = x_uia_launch("cmd.exe /c exit 0"); break;
		case 5u: result = x_uia_events_subscribe_focus_changed(events); break;
		default: result = x_uia_events_subscribe_property_changed(events, element, UIA_NamePropertyId); break;
		}
		ok = !g_x_uia_test_fail_calloc && handle == NULL && result == 0 &&
			xrtErrorKind(xrtGetError()) == XERR_MEMORY;
		g_x_uia_test_fail_calloc = 0;
		if ( handle != NULL ) {
			if ( operation == 0u ) x_uia_element_release(handle);
			if ( operation == 1u ) x_uia_condition_release(handle);
			if ( operation == 2u ) x_uia_test_window_release(handle);
			if ( operation == 3u ) x_uia_events_release(handle);
			if ( operation == 4u ) x_uia_process_release(handle);
		}
		if ( events != NULL && !x_uia_events_release(events) ) { ok = false; }
		if ( !ok ) { fprintf(stderr, "UIA handle allocation failed: operation=%u\n", operation); return 0; }
	}
	return x_uia_element_valid(element);
}

/* 自建 Edit 的五个属性和空轮询：text 的协议诊断另占一次分配。 */
static int x_uia_test_owned_budgets(void* element)
{
	char* (*getters[])(void*) = {x_uia_element_name_owned,
		x_uia_element_automation_id_owned, x_uia_element_class_name_owned,
		x_uia_element_value_owned, x_uia_element_text_owned};
	void* events = x_uia_events_create();
	char* held = NULL;
	char* result = NULL;
	int ok = 0;
	if ( events == NULL || !x_uia_element_set_value(element, "owned text") ) { goto done; }
	for ( unsigned operation = 0u; operation < 6u; ++operation ) {
		held = operation < 5u ? getters[operation](element) : x_uia_events_poll_json_owned(events, 0);
		if ( held == NULL ) { goto done; }
		/* text 先尝试 TextPattern，缺失协议诊断与 Value 文本各占一个分配点。 */
		unsigned needed = operation == 4u ? 2u : 1u;
		for ( unsigned budget = 0u; budget <= needed; ++budget ) {
			bool triggered, memory_error;
			xrtClearError();
			if ( !xrtMemDebugFailAfter(budget) ) { goto done; }
			result = operation < 5u ? getters[operation](element) : x_uia_events_poll_json_owned(events, 0);
			triggered = xrtMemDebugFailTriggered();
			memory_error = xrtErrorKind(xrtGetError()) == XERR_MEMORY;
			xrtMemDebugFailClear();
			if ( budget < needed ? (!triggered || !memory_error || result != NULL) :
				(triggered || memory_error || result == NULL || result == held || strcmp(result, held) != 0) ) {
				fprintf(stderr, "UIA owned budget failed: operation=%u budget=%u\n", operation, budget);
				goto done;
			}
			xrtFree(result); result = NULL;
		}
		xrtFree(held); held = NULL;
	}
	ok = 1;
done:
	xrtMemDebugFailClear();
	xrtFree(result);
	xrtFree(held);
	if ( events != NULL && !x_uia_events_release(events) ) { ok = 0; }
	return ok;
}

int main(void)
{
	void* pWindow = x_uia_test_window_create();
	void* pRoot = NULL;
	void* pName = NULL;
	void* pClass = NULL;
	void* pType = NULL;
	void* pBoth = NULL;
	void* pFound = NULL;
	void* pPath = NULL;
	void* pEvents = NULL;
	xvalue* pChildren = NULL;
	ptr pChild = NULL;
	XUiaThreadCheck threadCheck;
	char owner_diagnostic[sizeof(g_x_uia_error)];
	HANDLE worker = NULL;
	MSG quit_message;
	DWORD poll_started;
	UINT_PTR poll_timer;
	int iResult = 1;

	if ( pWindow == NULL || !x_uia_test_window_valid(pWindow) ) { goto cleanup; }
	pRoot = x_uia_from_hwnd(x_uia_test_window_hwnd(pWindow));
	if ( pRoot == NULL || !x_uia_element_valid(pRoot) ) { goto cleanup; }
	pName = x_uia_by_name("missing");
	if ( pName == NULL || !x_uia_condition_valid(pName) ) {
		fprintf(stderr, "by_name failed: %s; VARIANT=%zu\n", x_uia_last_error(), sizeof(VARIANT));
		goto cleanup;
	}
	if ( x_uia_by_name("\xC3\x28") != NULL ) { goto cleanup; }
	pClass = x_uia_by_class_name("Edit");
	pType = x_uia_by_control_type(50004);
	if ( pClass == NULL || pType == NULL ) { goto cleanup; }
	pBoth = x_uia_condition_and(pClass, pType);
	if ( pBoth == NULL ) { goto cleanup; }
	x_uia_condition_release(pClass); pClass = NULL;
	x_uia_condition_release(pType); pType = NULL;
	pFound = x_uia_element_find_first(pRoot, pBoth, 4, 1000);
	if ( pFound == NULL || !x_uia_element_valid(pFound) ) { goto cleanup; }
	{
		BSTR longText = SysAllocStringLen(NULL, 9001u);
		const char* converted;
		int valid;
		if ( longText == NULL ) { goto cleanup; }
		for ( size_t i = 0u; i < 9000u; ++i ) { longText[i] = L'x'; }
		longText[9000] = 0x4f60;
		converted = x_uia_bstr_to_text(longText);
		valid = converted != NULL && strlen(converted) == 9003u &&
			memcmp(converted + 9000u, "\xe4\xbd\xa0", 3u) == 0;
		SysFreeString(longText);
		if ( !valid ) {
			fprintf(stderr, "UIA long BSTR conversion lost data\n");
			goto cleanup;
		}
	}
	{
		WCHAR embedded[] = {L'x', L'\0', L'y'};
		WCHAR surrogate[] = {0xd800};
		BSTR invalid = SysAllocStringLen(embedded, 3u);
		if ( invalid == NULL ) { goto cleanup; }
		xrtClearError();
		if ( x_uia_bstr_to_text(invalid) != NULL || xrtGetError() == NULL ) {
			SysFreeString(invalid);
			goto cleanup;
		}
		SysFreeString(invalid);
		invalid = SysAllocStringLen(surrogate, 1u);
		if ( invalid == NULL ) { goto cleanup; }
		xrtClearError();
		if ( x_uia_bstr_to_text(invalid) != NULL || xrtGetError() == NULL ) {
			SysFreeString(invalid);
			goto cleanup;
		}
		SysFreeString(invalid);
		x_uia_set_error("");
	}
	{
		XUiaSnapshotContext snapshot;
		XUiaJsonBuilder json;
		char* longText = (char*)malloc(70001u);
		if ( longText == NULL ) { goto cleanup; }
		memset(&snapshot, 0, sizeof(snapshot));
		x_uia_json_init(&snapshot.json);
		x_uia_snapshot_append_bstr_property(&snapshot, NULL, "name", x_uia_test_long_name);
		if ( snapshot.json.overflow || snapshot.json.length < 9000u ) {
			fprintf(stderr, "UIA snapshot lost a long property\n");
			free(longText);
			goto cleanup;
		}
		memset(longText, 'q', 70000u);
		longText[70000u] = '\0';
		x_uia_json_init(&json);
		x_uia_json_append_escaped(&json, longText);
		free(longText);
		if ( json.overflow || json.length != 70002u ||
			json.data[0] != '"' || json.data[70001u] != '"' ||
			strlen(json.data) != 70002u ) {
			fprintf(stderr, "UIA JSON output hit the fixed buffer\n");
			goto cleanup;
		}
		{
			char* owned = (char*)x_uia_take_text(x_uia_json_finish(&json, "UIA JSON output limit"),
				&g_x_uia_json, &g_x_uia_json_capacity);
			if ( owned != json.data || g_x_uia_json != NULL || g_x_uia_json_capacity != 0u ) {
				fprintf(stderr, "UIA JSON ownership was copied instead of transferred\n");
				xrtFree(owned);
				goto cleanup;
			}
			x_uia_json_init(&json);
			x_uia_json_append_raw(&json, "new result");
			if ( strlen(owned) != 70002u || strcmp(json.data, "new result") != 0 ) {
				fprintf(stderr, "UIA transferred JSON was overwritten\n");
				xrtFree(owned);
				goto cleanup;
			}
			xrtFree(owned);
		}
		x_uia_json_init(&json);
		json.length = X_UIA_JSON_MAX_BYTES;
		x_uia_json_append_raw(&json, "x");
		if ( json.overflow != 1 ||
			strcmp(x_uia_json_finish(&json, "UIA JSON output limit"), "{}") != 0 ||
			xrtGetError() == NULL ) {
			fprintf(stderr, "UIA JSON output limit was not reported\n");
			goto cleanup;
		}
		x_uia_set_error("");
	}
	{
		char* owned = (char*)x_uia_element_snapshot_json_owned(pRoot, 2, 32);
		if ( owned == NULL || strstr(owned, "\"children\":") == NULL ||
			x_uia_element_cached_children_json(pRoot, 2, 32) == NULL ||
			strstr(owned, "\"children\":") == NULL ) {
			fprintf(stderr, "UIA snapshot ownership failed\n");
			xrtFree(owned);
			goto cleanup;
		}
		xrtFree(owned);
		owned = (char*)x_uia_element_name_owned(pFound);
		if ( owned == NULL || x_uia_element_class_name(pFound) == NULL ||
			xrtGetError() != NULL ) {
			fprintf(stderr, "UIA BSTR ownership failed\n");
			xrtFree(owned);
			goto cleanup;
		}
		xrtFree(owned);
		xrtClearError();
		if ( x_uia_element_name_owned(NULL) != NULL || xrtGetError() == NULL ) {
			fprintf(stderr, "UIA owned getter lost its error\n");
			goto cleanup;
		}
		xrtClearError();
	}
	{
		char shortField[512];
		const char* locator;
		xrtClearError();
		/* 伪 getter 只依赖输出参数，不访问元素。 */
		if ( x_uia_current_bstr_to_buffer((IUIAutomationElement*)pFound,
			x_uia_test_long_name, shortField, sizeof(shortField)) != 0 ||
			shortField[0] != '\0' || xrtGetError() == NULL ) {
			fprintf(stderr, "UIA locator accepted a truncated property\n");
			goto cleanup;
		}
		xrtClearError();
		if ( x_uia_current_bstr_to_buffer((IUIAutomationElement*)pFound,
			x_uia_test_failed_name, shortField, sizeof(shortField)) != 0 ||
			xrtGetError() == NULL ) {
			fprintf(stderr, "UIA locator ignored a property failure\n");
			goto cleanup;
		}
		locator = x_uia_locator_for(pFound);
		if ( locator == NULL || strstr(locator, "\"classPath\":") == NULL ||
			xrtGetError() != NULL ) {
			fprintf(stderr, "UIA ordinary locator failed\n");
			goto cleanup;
		}
	}
	{
		char segment[64];
		char other_type[64];
		int ok = x_uia_locator_segment(segment, sizeof(segment), "A/B[0]%#", 50004, 2);
		int correct = ok && strcmp(segment, "A%2FB%5B0%5D%25%23#50004[2]") == 0;
		int small = x_uia_locator_segment(segment, 6u, "A/B", 50004, 2);
		int distinct = x_uia_locator_segment(other_type, sizeof(other_type),
			"A/B[0]%#", 50000, 2) && strcmp(other_type,
			"A%2FB%5B0%5D%25%23#50000[2]") == 0;
		if ( !correct || small || !distinct ) {
			fprintf(stderr, "UIA class path segment failed: correct=%d small=%d distinct=%d\n",
				correct, small, distinct);
			goto cleanup;
		}
	}
	{
		XUiaLocatorInfo info;
		if ( !x_uia_locator_read_info(((XUiaElement*)pFound)->element, &info) ) {
			fprintf(stderr, "UIA class path generation failed\n");
			goto cleanup;
		}
		pPath = x_uia_element_find_class_path(pRoot, info.class_path, info.process_id, 0);
		if ( pPath == NULL ||
			!x_uia_locator_same_element(((XUiaElement*)pPath)->element,
				((XUiaElement*)pFound)->element) ) {
			fprintf(stderr, "UIA class path round-trip failed: %s\n", x_uia_last_error());
			goto cleanup;
		}
		x_uia_element_release(pPath); pPath = NULL;
		{
			char malformed[sizeof(info.class_path)];
			int length = snprintf(malformed, sizeof(malformed), "%s/Edit#50004[",
				info.class_path);
			if ( length < 0 || length >= (int)sizeof(malformed) ) { goto cleanup; }
			pPath = x_uia_element_find_class_path(pRoot, malformed, 0, 0);
			if ( pPath != NULL || xrtGetError() == NULL ) {
				fprintf(stderr, "UIA malformed class path was not rejected\n");
				goto cleanup;
			}
		}
		pPath = x_uia_element_find_class_path(pRoot, info.class_path, 2147483647, 0);
		if ( pPath != NULL || xrtGetError() != NULL || x_uia_last_error()[0] != '\0' ) {
			fprintf(stderr, "UIA class path ignored process ID\n");
			goto cleanup;
		}
	}
	if ( x_uia_shutdown() != 0 ) { goto cleanup; }
	memset(&threadCheck, 0, sizeof(threadCheck));
	threadCheck.root = pRoot;
	pEvents = x_uia_events_create();
	if ( pEvents == NULL ) { goto cleanup; }
	threadCheck.events = pEvents;
	memcpy(owner_diagnostic, g_x_uia_error, sizeof(owner_diagnostic));
	worker = CreateThread(NULL, 0, x_uia_check_other_thread, &threadCheck, 0, NULL);
	if ( worker == NULL || WaitForSingleObject(worker, 2000u) != WAIT_OBJECT_0 ||
		threadCheck.valid != 0 || threadCheck.shutdown != 0 || threadCheck.release_events != 0 ||
		threadCheck.desktop != NULL || threadCheck.process != NULL ||
		memcmp(owner_diagnostic, g_x_uia_error, sizeof(owner_diagnostic)) != 0 ||
		!x_uia_element_valid(pRoot) ||
		!x_uia_events_valid(pEvents) || !x_uia_events_release(pEvents) ) { goto cleanup; }
	pEvents = NULL;
	for ( int i = 0; i < 8; ++i ) {
		pEvents = x_uia_events_create();
		if ( pEvents == NULL || !x_uia_events_subscribe_focus_changed(pEvents) ||
			!x_uia_events_subscribe_property_changed(pEvents, pFound, UIA_NamePropertyId) ||
			!x_uia_test_window_focus_edit(pWindow) ) { goto cleanup; }
		if ( !x_uia_events_release(pEvents) ) { goto cleanup; }
		pEvents = NULL;
	}
	pEvents = x_uia_events_create();
	if ( pEvents == NULL || !x_uia_events_subscribe_focus_changed(pEvents) ||
		!x_uia_test_window_focus_button(pWindow) ||
		strstr(x_uia_events_poll_json(pEvents, 1000), "\"type\":\"focus\"") == NULL ||
		!x_uia_events_release(pEvents) ) { goto cleanup; }
	pEvents = NULL;
	/* 事件轮询不能吞掉宿主线程请求退出的 WM_QUIT。 */
	pEvents = x_uia_events_create();
	if ( pEvents == NULL ) { goto cleanup; }
	PeekMessageA(&quit_message, NULL, 0u, 0u, PM_NOREMOVE);
	PostQuitMessage(7);
	if ( strcmp(x_uia_events_poll_json(pEvents, 100), "{}") != 0 ||
		!PeekMessageA(&quit_message, NULL, WM_QUIT, WM_QUIT, PM_NOREMOVE) ||
		quit_message.wParam != 7u ) {
		fprintf(stderr, "UIA event poll consumed the host quit message\n");
		goto cleanup;
	}
	PeekMessageA(&quit_message, NULL, WM_QUIT, WM_QUIT, PM_REMOVE);
	if ( !x_uia_events_release(pEvents) ) { goto cleanup; }
	pEvents = NULL;
	/* 消息回调在轮询中释放队列后，轮询必须及时安全地返回。 */
	pEvents = x_uia_events_create();
	if ( pEvents == NULL ) { goto cleanup; }
	g_x_uia_poll_release_queue = pEvents;
	g_x_uia_poll_release_result = 0;
	poll_timer = SetTimer(NULL, 0u, 1u, x_uia_test_release_during_poll);
	if ( poll_timer == 0u ) {
		fprintf(stderr, "UIA reentrant release timer failed: %lu\n", (unsigned long)GetLastError());
		goto cleanup;
	}
	poll_started = GetTickCount();
	if ( strcmp(x_uia_events_poll_json(pEvents, 2000), "{}") != 0 ||
		g_x_uia_poll_release_result != 1 || GetTickCount() - poll_started > 1000u ) {
		fprintf(stderr, "UIA event poll did not survive reentrant release\n");
		KillTimer(NULL, poll_timer);
		if ( g_x_uia_poll_release_result ) { pEvents = NULL; }
		goto cleanup;
	}
	pEvents = NULL;
	pChildren = x_uia_element_children(pRoot);
	if ( pChildren == NULL || xrtValueType(pChildren) != XVALUE_ARRAY ||
		xrtValueCount(pChildren) == 0u ||
		!xrtValueGetPointer(xrtValueArrayGet(pChildren, 0u), &pChild) ||
		pChild == NULL ) {
		fprintf(stderr, "UIA element read after event poll failed: %s\n", x_uia_last_error());
		if ( pChildren != NULL ) { xrtValueRelease(pChildren); }
		goto cleanup;
	}
	/* The array stores raw pointers; the caller owns and releases each wrapper. */
	for ( size_t i = 0u; i < xrtValueCount(pChildren); ++i ) {
		ptr child = NULL;
		if ( xrtValueGetPointer(xrtValueArrayGet(pChildren, i), &child) && child != NULL ) {
			x_uia_element_release(child);
		}
	}
	xrtValueRelease(pChildren);
	if ( !x_uia_test_memory_fallback() || !x_uia_test_bstr_allocation(pFound) ||
		!x_uia_test_handle_allocation(pWindow, pFound) ||
		!x_uia_test_owned_budgets(pFound) ) { goto cleanup; }
	/* 关闭后的测试窗口不得把失败的焦点消息报告为成功。 */
	if ( !x_uia_test_window_close(pWindow) || x_uia_test_window_valid(pWindow) ||
		x_uia_test_window_focus_edit(pWindow) ||
		x_uia_test_window_focus_button(pWindow) ) {
		fprintf(stderr, "UIA closed window accepted a focus request\n");
		goto cleanup;
	}
	iResult = 0;

cleanup:
	if ( pPath != NULL ) { x_uia_element_release(pPath); }
	if ( pEvents != NULL ) { x_uia_events_release(pEvents); }
	if ( worker != NULL ) { CloseHandle(worker); }
	if ( pFound != NULL ) { x_uia_element_release(pFound); }
	if ( pBoth != NULL ) { x_uia_condition_release(pBoth); }
	if ( pClass != NULL ) { x_uia_condition_release(pClass); }
	if ( pType != NULL ) { x_uia_condition_release(pType); }
	if ( pName != NULL ) { x_uia_condition_release(pName); }
	if ( pRoot != NULL ) { x_uia_element_release(pRoot); }
	if ( pWindow != NULL ) {
		x_uia_test_window_close(pWindow);
		x_uia_test_window_release(pWindow);
	}
	if ( !x_uia_shutdown() ) { iResult = 1; }
	if ( iResult == 0 ) { puts("uia C smoke: PASS"); }
	return iResult;
}
