#include "../../uia/include/uia.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
    char* end = NULL;
    long pid;
    void *desktop = NULL, *process = NULL, *edit_class = NULL, *edit_type = NULL;
    void *edit_pair = NULL, *edit_filter = NULL, *edit = NULL;
    void *button_class = NULL, *button_type = NULL, *button_pair = NULL;
    void *button_filter = NULL, *button = NULL, *wrong_process = NULL;
    void *wrong_filter = NULL, *wrong_edit = NULL;
    char* text = NULL;
    int ok = 0;

    if (argc != 2) { return 2; }
    pid = strtol(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || pid <= 0 || pid >= INT_MAX) { return 2; }

    desktop = x_uia_desktop();
    process = x_uia_by_process_id((int)pid);
    edit_class = x_uia_by_class_name("Edit");
    edit_type = x_uia_by_control_type(50004);
    edit_pair = edit_class && edit_type ? x_uia_condition_and(edit_class, edit_type) : NULL;
    edit_filter = edit_pair && process ? x_uia_condition_and(edit_pair, process) : NULL;
    edit = desktop && edit_filter ? x_uia_element_find_first(desktop, edit_filter, 4, 5000) : NULL;
    if (!edit || !x_uia_element_set_value(edit, "C cross-process text")) { goto cleanup; }
    text = x_uia_element_text_owned(edit);
    if (!text || strcmp(text, "C cross-process text") != 0) { goto cleanup; }

    button_class = x_uia_by_class_name("Button");
    button_type = x_uia_by_control_type(50000);
    button_pair = button_class && button_type ? x_uia_condition_and(button_class, button_type) : NULL;
    button_filter = button_pair && process ? x_uia_condition_and(button_pair, process) : NULL;
    button = button_filter ? x_uia_element_find_first(desktop, button_filter, 4, 5000) : NULL;
    if (!button || !x_uia_element_click(button, 5000)) { goto cleanup; }

    wrong_process = x_uia_by_process_id(INT_MAX);
    wrong_filter = wrong_process ? x_uia_condition_and(edit_pair, wrong_process) : NULL;
    wrong_edit = wrong_filter ? x_uia_element_find_first(desktop, wrong_filter, 4, 0) : NULL;
    if (wrong_edit) { goto cleanup; }
    ok = 1;

cleanup:
    if (!ok) { fprintf(stderr, "ui.auto C cross-process: %s\n", x_uia_last_error()); }
    xrtFree(text);
    if (wrong_edit) x_uia_element_release(wrong_edit);
    if (wrong_filter) x_uia_condition_release(wrong_filter);
    if (wrong_process) x_uia_condition_release(wrong_process);
    if (button) x_uia_element_release(button);
    if (button_filter) x_uia_condition_release(button_filter);
    if (button_pair) x_uia_condition_release(button_pair);
    if (button_type) x_uia_condition_release(button_type);
    if (button_class) x_uia_condition_release(button_class);
    if (edit) x_uia_element_release(edit);
    if (edit_filter) x_uia_condition_release(edit_filter);
    if (edit_pair) x_uia_condition_release(edit_pair);
    if (edit_type) x_uia_condition_release(edit_type);
    if (edit_class) x_uia_condition_release(edit_class);
    if (process) x_uia_condition_release(process);
    if (desktop) x_uia_element_release(desktop);
    if (!x_uia_shutdown()) { ok = 0; }
    if (ok) puts("ui.auto C cross-process ok");
    return ok ? 0 : 1;
}
