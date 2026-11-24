#ifndef FOREIGN_PACK_OWNED_CALLBACK_H
#define FOREIGN_PACK_OWNED_CALLBACK_H
#include <__xl_runtime/xrt_decl.h>
#include <__xl_runtime/xl6_foreign_view_abi.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct GpOwner {xrtownershipref reference;bool (*hold)(const void*);bool (*drop)(const void*);} GpOwner;
typedef struct GpDesc {int seed;int (*value)(int,void*);void (*notice)(void*,int);void* user;} GpDesc;
typedef struct GpNested {GpDesc inner;} GpNested;
typedef struct GpReceiver {
    int seed;
    int (*value)(const struct GpReceiver*,int);
    void (*notice)(struct GpReceiver*,int);
    void* user;
} GpReceiver;
int gp_receiver_install(const GpReceiver*,const GpOwner*);
int gp_receiver_dispatch(int);
int gp_receiver_clear(void);
typedef struct GpRoot GpRoot;
struct GpRoot {int (*install)(GpRoot*,const GpDesc*,const GpOwner*);GpOwner failed_owner;};
int gp_install(const GpDesc*,const GpOwner*);
int gp_dispatch(int);
int gp_clear(void);
int gp_two(const GpDesc*,const GpOwner*,const GpDesc*,const GpOwner*);
int gp_optional(const GpDesc*,const GpOwner*);
int gp_nested(const GpNested*,const GpOwner*);
int gp_reject(const GpDesc*,GpRoot**,const GpOwner*);
int gp_failure_roots(void);
int gp_root(GpRoot**);
void gp_root_drop(GpRoot*);
extern const XL6_ForeignViewDescriptorV1 gpPolicy;
int gp_installs(void);
int gp_active(void);
void gp_fail_after(int);
int gp_fail_reset(void);
void gp_heap(size_t*,size_t*,uint64_t*,uint64_t*,uint64_t*);
#ifdef __cplusplus
}
#endif
#endif
