#ifndef XL6_XGE3_PACKAGE_SHAPE_FACTORY_H
#define XL6_XGE3_PACKAGE_SHAPE_FACTORY_H
#include <xge3_runtime.h>
int shape_cpu_proxy(xui_proxy*);
int shape_cpu_font(xui_proxy,xui_font*);
void shape_cpu_arm(int);
int shape_cpu_observed(void);
int shape_cpu_clear_oom(void);
void shape_cpu_ready(void);
int shape_cpu_finish(void);
int shape_cpu_verify(void);
#endif
