/* Windows UI Automation 的动态值边界：元素集合必须返回当前 XRT xvalue 数组。 */
#include "../include/uia.h"
#include <stdio.h>

#include "../x_uia_artifact.c"

typedef struct XUiaThreadCheck {
	void* root;
	int valid;
	int shutdown;
	void* desktop;
} XUiaThreadCheck;

static DWORD WINAPI x_uia_check_other_thread(void* context)
{
	XUiaThreadCheck* check = (XUiaThreadCheck*)context;
	check->valid = x_uia_element_valid(check->root);
	check->shutdown = x_uia_shutdown();
	check->desktop = x_uia_desktop();
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
	HANDLE worker = NULL;
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
		int ok = x_uia_locator_segment(segment, sizeof(segment), "A/B[0]%#", 50004, 2);
		int correct = ok && strcmp(segment, "A%2FB%5B0%5D%25%23#50004[2]") == 0;
		int small = x_uia_locator_segment(segment, 6u, "A/B", 50004, 2);
		if ( !correct || small ) {
			fprintf(stderr, "UIA class path escaping failed: correct=%d small=%d\n", correct, small);
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
		pPath = x_uia_element_find_class_path(pRoot, info.class_path, 2147483647, 0);
		if ( pPath != NULL || xrtGetError() != NULL || x_uia_last_error()[0] != '\0' ) {
			fprintf(stderr, "UIA class path ignored process ID\n");
			goto cleanup;
		}
	}
	if ( x_uia_shutdown() != 0 ) { goto cleanup; }
	memset(&threadCheck, 0, sizeof(threadCheck));
	threadCheck.root = pRoot;
	worker = CreateThread(NULL, 0, x_uia_check_other_thread, &threadCheck, 0, NULL);
	if ( worker == NULL || WaitForSingleObject(worker, 2000u) != WAIT_OBJECT_0 ||
		threadCheck.valid != 0 || threadCheck.shutdown != 0 || threadCheck.desktop != NULL ||
		!x_uia_element_valid(pRoot) ) { goto cleanup; }
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
	pChildren = x_uia_element_children(pRoot);
	if ( pChildren == NULL || xrtValueType(pChildren) != XVALUE_ARRAY ||
		xrtValueCount(pChildren) == 0u ||
		!xrtValueGetPointer(xrtValueArrayGet(pChildren, 0u), &pChild) ||
		pChild == NULL ) {
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
