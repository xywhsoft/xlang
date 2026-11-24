#ifndef FOREIGN_CALLBACK_ELEMENTS_H
#define FOREIGN_CALLBACK_ELEMENTS_H

#include "xge3_callback_elements.h"

typedef int (*probe_counted_elements)(const xui_mesh_vertex_t*,size_t,void*);

int xl_test_proxy_mesh(xui_proxy,int);
int xl_test_proxy_clipboard_items(xui_proxy);
int xl_test_counted_elements(probe_counted_elements,void*,int);

#endif
