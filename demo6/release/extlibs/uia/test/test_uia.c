/* Windows UI Automation 的动态值边界：元素集合必须返回当前 XRT xvalue 数组。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include <stdio.h>

#include "../x_uia_artifact.c"

int main(void)
{
	void* pWindow = x_uia_test_window_create();
	void* pRoot = NULL;
	xvalue* pChildren = NULL;
	ptr pChild = NULL;
	int iResult = 1;

	if ( pWindow == NULL || !x_uia_test_window_valid(pWindow) ) { goto cleanup; }
	pRoot = x_uia_from_hwnd(x_uia_test_window_hwnd(pWindow));
	if ( pRoot == NULL || !x_uia_element_valid(pRoot) ) { goto cleanup; }
	pChildren = x_uia_element_children(pRoot);
	if ( pChildren == NULL || xrtValueType(pChildren) != XVALUE_ARRAY ||
		xrtValueCount(pChildren) == 0u ||
		!xrtValueGetPointer(xrtValueArrayGet(pChildren, 0u), &pChild) ||
		pChild == NULL ) {
		if ( pChildren != NULL ) { xrtValueRelease(pChildren); }
		goto cleanup;
	}
	/* xvalue pointer entries are borrowed handles; release every wrapped child. */
	for ( size_t i = 0u; i < xrtValueCount(pChildren); ++i ) {
		ptr child = NULL;
		if ( xrtValueGetPointer(xrtValueArrayGet(pChildren, i), &child) && child != NULL ) {
			x_uia_element_release(child);
		}
	}
	xrtValueRelease(pChildren);
	iResult = 0;

cleanup:
	if ( pRoot != NULL ) { x_uia_element_release(pRoot); }
	if ( pWindow != NULL ) {
		x_uia_test_window_close(pWindow);
		x_uia_test_window_release(pWindow);
	}
	x_uia_shutdown();
	if ( iResult == 0 ) { puts("uia C smoke: PASS"); }
	return iResult;
}
