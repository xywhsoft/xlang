#ifndef XL6_TEST_FOREIGN_OUTPUT_RETAIN_H
#define XL6_TEST_FOREIGN_OUTPUT_RETAIN_H
#include <__xl_runtime/xl6_foreign_view_abi.h>
#include <xrt_decl.h>
typedef struct OutputRetainNode {int refs,mode,value;} OutputRetainNode;
extern const XL6_ForeignViewDescriptorV1 outputRetainPolicy;
int output_retain_create(int,OutputRetainNode**);
int output_retain_hold(OutputRetainNode*);
int output_retain_hold_positive(OutputRetainNode*);
int output_retain_drop(OutputRetainNode*);
int output_retain_values(OutputRetainNode*,OutputRetainNode**,int*,OutputRetainNode**,xstrview*);
int output_retain_value(OutputRetainNode*);
int output_retain_refs(OutputRetainNode*);
int output_retain_live(void);
int output_retain_calls(void);
void output_retain_fail_after(int);
void output_retain_reset(void);
#endif
