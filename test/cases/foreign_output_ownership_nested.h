#ifndef XL6_TEST_FOREIGN_OUTPUT_OWNERSHIP_NESTED_H
#define XL6_TEST_FOREIGN_OUTPUT_OWNERSHIP_NESTED_H
#include <__xl_runtime/xrt_decl.h>
#include <__xl_runtime/xl6_foreign_view.h>
typedef struct NestedOutputRoot {int value;} NestedOutputRoot;
static inline int nested_output_create(NestedOutputRoot** output)
{
    *output=(NestedOutputRoot*)xrtCalloc(1,sizeof(**output));
    if(!*output)return -1;
    (*output)->value=97;return 0;
}
static inline void nested_output_free(NestedOutputRoot* root){xrtFree(root);}
static inline bool nested_output_destroy(void* raw){nested_output_free((NestedOutputRoot*)raw);return true;}
static const XL6_ForeignViewDescriptorV1 nestedOutputPolicy={
    sizeof(nestedOutputPolicy),XL6_FOREIGN_VIEW_V1_TAG,"nested.output.root",0,1,NULL,NULL,
    nested_output_destroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
static inline int nested_output_self(NestedOutputRoot* root,NestedOutputRoot** output)
{*output=root;return 0;}
static inline int nested_output_name(NestedOutputRoot* root,const char** output)
{if(!root)return -1;*output="nested UTF-8 \xe4\xbd\xa0";return 0;}
static inline int nested_output_value(NestedOutputRoot* root){return root->value;}
#endif
