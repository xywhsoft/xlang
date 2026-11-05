#ifndef FOREIGN_PACK_POD_H
#define FOREIGN_PACK_POD_H
#include <__xl_runtime/xl6_foreign_view_abi.h>
#include <xrt_decl.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct PackPodPoint {float x,y;} PackPodPoint;
typedef struct PackPodBox {PackPodPoint origin;int width,height;} PackPodBox;
typedef struct PackPodDesc {uint32_t size;int factor;PackPodPoint point;PackPodBox box;} PackPodDesc;
typedef struct PackPodRoot PackPodRoot;
struct PackPodRoot {int value;int(*evaluate)(PackPodRoot*,const PackPodDesc*,int*);};
extern const XL6_ForeignViewDescriptorV1 packPodPolicy;
int pack_pod_create(PackPodRoot**);
void pack_pod_drop(PackPodRoot*);
int pack_pod_call(PackPodRoot*,const PackPodDesc*,int*);
int pack_pod_free(const PackPodDesc*,int*);
int pack_pod_calls(void);
void pack_pod_fail_after(int);
void pack_pod_reset(void);
#ifdef __cplusplus
}
#endif
#endif
