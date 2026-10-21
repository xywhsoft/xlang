#include "../../extlibs/uia/include/uia.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	void* window = x_uia_test_window_create();
	void* root = NULL;
	void* by_class = NULL;
	void* by_type = NULL;
	void* by_process = NULL;
	void* pair = NULL;
	void* condition = NULL;
	void* edit = NULL;
	void* button_class = NULL;
	void* button_type = NULL;
	void* button_condition = NULL;
	void* button = NULL;
	int ok = 0;

	if ( window == NULL || !x_uia_test_window_valid(window) ) { goto cleanup; }
	root = x_uia_from_hwnd(x_uia_test_window_hwnd(window));
	by_class = x_uia_by_class_name("Edit");
	by_type = x_uia_by_control_type(50004);
	by_process = x_uia_by_process_id((int)GetCurrentProcessId());
	if ( root == NULL || by_class == NULL || by_type == NULL ||
		by_process == NULL ) { goto cleanup; }
	pair = x_uia_condition_and(by_class, by_type);
	condition = pair != NULL ? x_uia_condition_and(pair, by_process) : NULL;
	edit = condition != NULL ? x_uia_element_find_first(root, condition, 4, 0) : NULL;
	if ( edit == NULL || !x_uia_element_set_value(edit, "xlang UI automation") ||
		strcmp(x_uia_element_text(edit), "xlang UI automation") != 0 ) {
		goto cleanup;
	}
	printf("edit text:  %s\n", x_uia_element_text(edit));
	button_class = x_uia_by_class_name("Button");
	button_type = x_uia_by_control_type(50000);
	button_condition = button_class != NULL && button_type != NULL ?
		x_uia_condition_and(button_class, button_type) : NULL;
	button = button_condition != NULL ?
		x_uia_element_find_first(root, button_condition, 4, 0) : NULL;
	if ( button == NULL || !x_uia_element_click(button, 5000) ||
		!x_uia_test_window_close(window) ) {
		goto cleanup;
	}
	puts("ui.auto example: PASS");
	ok = 1;

cleanup:
	if ( !ok ) { fprintf(stderr, "ui.auto C: %s\n", x_uia_last_error()); }
	if ( button != NULL ) { x_uia_element_release(button); }
	if ( button_condition != NULL ) { x_uia_condition_release(button_condition); }
	if ( button_type != NULL ) { x_uia_condition_release(button_type); }
	if ( button_class != NULL ) { x_uia_condition_release(button_class); }
	if ( edit != NULL ) { x_uia_element_release(edit); }
	if ( condition != NULL ) { x_uia_condition_release(condition); }
	if ( pair != NULL ) { x_uia_condition_release(pair); }
	if ( by_process != NULL ) { x_uia_condition_release(by_process); }
	if ( by_type != NULL ) { x_uia_condition_release(by_type); }
	if ( by_class != NULL ) { x_uia_condition_release(by_class); }
	if ( root != NULL ) { x_uia_element_release(root); }
	if ( window != NULL ) { x_uia_test_window_release(window); }
	x_uia_shutdown();
	return ok ? 0 : 1;
}
