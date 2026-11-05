#ifndef FOREIGN_XGE3_CHECKED_DRAG_H
#define FOREIGN_XGE3_CHECKED_DRAG_H
#include <xge3_runtime.h>
int binding_drag_dispatch(xge_data_object data,uint32_t* output);
int binding_drag_last_status(void);
uint32_t binding_drag_last_effect(void);
void binding_drag_no_alloc(void);
int binding_drag_reset_alloc(void);
#endif
