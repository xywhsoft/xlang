#ifndef XL6_BINDING_OUTPUT_VIEW_H
#define XL6_BINDING_OUTPUT_VIEW_H
#include <__xl_runtime/xl6_foreign_view_abi.h>
#include <xrt_decl.h>
typedef struct BindingOutputViewBox BindingOutputViewBox;
typedef struct BindingOutputViewScope { BindingOutputViewBox* box; } BindingOutputViewScope;
extern const XL6_ForeignViewDescriptorV1 bindingOutputViewPolicy;
int binding_output_view_create(BindingOutputViewBox**);
void binding_output_view_destroy(BindingOutputViewBox*);
int binding_output_view_pair(xstrview*,int*,xbytesview*,size_t*);
int binding_output_view_nullable(int,xstrview*,xbytesview*);
int binding_output_view_required(int,xstrview*,xbytesview*);
void binding_output_view_void(xstrview*);
int binding_output_view_bytes(xbytesview*);
int binding_output_view_span(const uint8_t*,size_t,xstrview*,xbytesview*);
int binding_output_view_method(BindingOutputViewBox*,int,int,xstrview*,xbytesview*,int*);
int binding_output_view_end_count(void);
void binding_output_view_reset(void);
void binding_output_view_counter_begin(void);
void binding_output_view_counter_end(void);
#endif
