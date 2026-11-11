#ifndef XL6_TEST_FOREIGN_FIXED_OUTPUT_H
#define XL6_TEST_FOREIGN_FIXED_OUTPUT_H
#include <__xl_runtime/xrt_decl.h>
#include <__xl_runtime/xl6_foreign_view.h>
#include <stdint.h>
#undef NDEBUG
#include <assert.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct FixedVertex {float x,y;} FixedVertex;
typedef struct FixedRoot FixedRoot;
struct FixedRoot {int (*read)(FixedRoot*,FixedVertex*);};
#if defined(XL6_TEST_FIXED_OUTPUT_DECL_ONLY)
int fixed_vertices(int,FixedVertex*);
void fixed_ints(int*);
int fixed_mixed(FixedVertex*,int*,float*);
int fixed_span(const uint8_t*,int,FixedVertex*,int);
int fixed_method(FixedRoot*,FixedVertex*);
int fixed_root(FixedRoot**);
void fixed_root_free(FixedRoot*);
unsigned fixed_calls(void);
void fixed_counters_begin(void);
void fixed_counters_end(void);
extern const XL6_ForeignViewDescriptorV1 fixedRootPolicy;
#else
static unsigned fixedOutputCalls;
static xmemdebugsnapshot fixedOutputBefore;
static inline int fixed_vertices(int code,FixedVertex* out)
{
    ++fixedOutputCalls;
    for(int i=0;i<4;++i){out[i].x=(float)(i*2+1);out[i].y=(float)(i*2+2);}
    return code;
}
static inline void fixed_ints(int* out)
{++fixedOutputCalls;for(int i=0;i<3;++i)out[i]=i+10;}
static inline int fixed_mixed(FixedVertex* vertices,int* tag,float* weights)
{fixed_vertices(0,vertices);*tag=91;for(int i=0;i<3;++i)weights[i]=(float)(i+1);return 0;}
static inline int fixed_span(const uint8_t* data,int count,FixedVertex* out,int code)
{assert(data && count==3 && data[0]=='a' && !data[1] && data[2]=='b');return fixed_vertices(code,out);}
static inline int fixed_method(FixedRoot* root,FixedVertex* out)
{assert(root);return fixed_vertices(0,out);}
static inline void fixed_root_free(FixedRoot* root){xrtFree(root);}
static inline bool fixed_root_destroy(void* root){fixed_root_free((FixedRoot*)root);return true;}
static const XL6_ForeignViewDescriptorV1 fixedRootPolicy={sizeof(XL6_ForeignViewDescriptorV1),XL6_FOREIGN_VIEW_V1_TAG,
    "fixed.output.root",0,1,NULL,NULL,fixed_root_destroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
static inline int fixed_root(FixedRoot** out)
{*out=(FixedRoot*)xrtMalloc(sizeof(FixedRoot));if(!*out)return -3;(*out)->read=fixed_method;return 0;}
static inline unsigned fixed_calls(void){return fixedOutputCalls;}
static inline void fixed_counters_begin(void){xrtMemDebugSnapshot(&fixedOutputBefore);}
static inline void fixed_counters_end(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(fixedOutputBefore.AllocCount==after.AllocCount && fixedOutputBefore.FreeCount==after.FreeCount &&
        fixedOutputBefore.ReallocCount==after.ReallocCount);
}
#endif
#ifdef __cplusplus
}
#endif
#endif
