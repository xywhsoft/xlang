#ifndef XLANG_UIA_H
#define XLANG_UIA_H

#if !defined(_WIN32)
#error "uia is supported only on Windows"
#endif

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Handles returned by create/find are owned by the caller. */
/* Text results borrow a process-global buffer and expire at the next UIA call. */
int x_uia_shutdown(void);
const char* x_uia_version(void);
const char* x_uia_last_error(void);
void* x_uia_launch(const char* command);
int x_uia_process_valid(void* handle);
int x_uia_process_id(void* handle);
int x_uia_process_close(void* handle);
int x_uia_process_release(void* handle);
void* x_uia_test_window_create(void);
int x_uia_test_window_valid(void* handle);
void* x_uia_test_window_hwnd(void* handle);
void* x_uia_test_window_edit_hwnd(void* handle);
void* x_uia_test_window_button_hwnd(void* handle);
void* x_uia_test_window_checkbox_hwnd(void* handle);
int x_uia_test_window_focus_edit(void* handle);
int x_uia_test_window_focus_button(void* handle);
int x_uia_test_window_close(void* handle);
int x_uia_test_window_release(void* handle);
void* x_uia_events_create(void);
int x_uia_events_valid(void* handle);
int x_uia_events_subscribe_focus_changed(void* handle);
int x_uia_events_subscribe_property_changed(void* handle, void* element_handle, int property_id);
const char* x_uia_events_poll_json(void* handle, int timeout_ms);
int x_uia_events_release(void* handle);
void* x_uia_desktop(void);
void* x_uia_focused(void);
void* x_uia_from_hwnd(void* hwnd);
void* x_uia_from_point(int x, int y);
int x_uia_element_release(void* handle);
int x_uia_element_valid(void* handle);
const char* x_uia_element_name(void* handle);
const char* x_uia_element_automation_id(void* handle);
const char* x_uia_element_class_name(void* handle);
int x_uia_element_control_type(void* handle);
void* x_uia_element_hwnd(void* handle);
const char* x_uia_element_rect_json(void* handle);
const char* x_uia_element_snapshot_json(void* handle, int depth, int max_nodes);
const char* x_uia_element_cached_children_json(void* handle, int depth, int max_nodes);
const char* x_uia_locator_for(void* handle);
const char* x_uia_focused_locator(void);
void* x_uia_by_name(const char* name);
void* x_uia_by_automation_id(const char* automation_id);
void* x_uia_by_class_name(const char* class_name);
void* x_uia_by_control_type(int control_type);
void* x_uia_by_process_id(int process_id);
void* x_uia_condition_and(void* left, void* right);
void* x_uia_condition_or(void* left, void* right);
void* x_uia_condition_not(void* handle);
int x_uia_condition_release(void* handle);
int x_uia_condition_valid(void* handle);
void* x_uia_element_find_first(void* element_handle, void* condition_handle, int scope, int timeout_ms);
xvalue* x_uia_element_find_all(void* element_handle, void* condition_handle, int scope, int timeout_ms);
void* x_uia_element_parent(void* handle);
xvalue* x_uia_element_children(void* handle);
int x_uia_element_set_focus(void* handle);
int x_uia_element_invoke(void* handle);
int x_uia_element_click(void* handle, int timeout_ms);
const char* x_uia_element_value(void* handle);
int x_uia_element_set_value(void* handle, const char* value);
const char* x_uia_element_text(void* handle);
int x_uia_element_toggle(void* handle);
int x_uia_element_toggle_state(void* handle);
double x_uia_element_range_value(void* handle);
double x_uia_element_range_min(void* handle);
double x_uia_element_range_max(void* handle);
int x_uia_element_set_range_value(void* handle, double value);
int x_uia_element_scroll(void* handle, int horizontal, int vertical);
int x_uia_element_close_window(void* handle);
int x_uia_element_wait_window_closed(void* handle, int timeout_ms);
#ifdef __cplusplus
}
#endif

#endif
