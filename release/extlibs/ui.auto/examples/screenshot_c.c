#define XRT_MODULE_JSON_READ
#include "../../uia/include/uia.h"
#include "../../opencv/include/opencv.h"
#include <stdio.h>
#include <string.h>

static int rect_number(xvalue* rect, const char* key, int* out)
{
	int64 value = 0;
	xvalue* field = xrtValueObjectGet(rect, (xstrview){key, strlen(key)});
	if ( field == NULL || !xrtValueGetInt(field, &value) ) { return 0; }
	*out = (int)value;
	return (int64)*out == value;
}

int main(void)
{
	void* window = x_uia_test_window_create();
	void* edit = NULL;
	void* image = NULL;
	void* parent = NULL;
	void* saved = NULL;
	xvalue* rect = NULL;
	const char* json;
	int left, top, width, height;
	int ok = 0;

	if ( window == NULL || !x_uia_test_window_valid(window) ) { goto cleanup; }
	edit = x_uia_from_hwnd(x_uia_test_window_edit_hwnd(window));
	if ( edit == NULL || !x_uia_element_set_value(edit, "capture 123") ) { goto cleanup; }

	/* 截取 UIA 控件矩形，而不是整张父窗口。 */
	json = x_uia_element_rect_json(edit);
	if ( json == NULL ) { goto cleanup; }
	rect = xrtJsonParse((xstrview){json, strlen(json)});
	if ( rect == NULL ||
		!rect_number(rect, "left", &left) || !rect_number(rect, "top", &top) ||
		!rect_number(rect, "width", &width) || !rect_number(rect, "height", &height) ||
		width <= 0 || height <= 0 ) { goto cleanup; }
	image = opencv_capture_rect_native(left, top, width, height);
	parent = opencv_capture_window_native(x_uia_test_window_hwnd(window));
	if ( image == NULL || parent == NULL ||
		opencv_width_native(image) >= opencv_width_native(parent) ||
		opencv_height_native(image) >= opencv_height_native(parent) ||
		!opencv_imwrite_native("screenshot_c.png", image) ) { goto cleanup; }
	saved = opencv_imread_native("screenshot_c.png", 1);
	if ( saved == NULL || opencv_width_native(saved) != opencv_width_native(image) ||
		opencv_height_native(saved) != opencv_height_native(image) ) { goto cleanup; }
	puts("ui.auto C screenshot ok");
	ok = 1;

cleanup:
	if ( !ok ) { fprintf(stderr, "ui.auto screenshot C: %s; %s\n", x_uia_last_error(), opencv_last_error_native()); }
	if ( saved != NULL ) { opencv_release_native(saved); }
	if ( parent != NULL ) { opencv_release_native(parent); }
	if ( image != NULL ) { opencv_release_native(image); }
	if ( rect != NULL ) { xrtValueRelease(rect); }
	if ( edit != NULL ) { x_uia_element_release(edit); }
	if ( window != NULL ) { x_uia_test_window_release(window); }
	x_uia_shutdown();
	return ok ? 0 : 1;
}
