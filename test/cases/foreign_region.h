#ifndef XL6_TEST_FOREIGN_REGION_H
#define XL6_TEST_FOREIGN_REGION_H
#include <xrt_decl.h>
#include <__xl_runtime/xl6_foreign_view_abi.h>
typedef struct RegionRoot RegionRoot;
typedef struct RegionDesc { int width,height; } RegionDesc;
typedef struct RegionRect { int x,y,width,height; } RegionRect;
typedef struct RegionVertex { float x,y; } RegionVertex;
typedef struct RegionQuad { RegionVertex vertices[4]; } RegionQuad;
typedef struct RegionInts { int values[4]; } RegionInts;
typedef struct RegionScope { RegionRoot* root; } RegionScope;
struct RegionRoot {
 int (*createRGBA)(RegionRoot*,int*,int,int,const void*,int,unsigned);
 int (*updateRGBA)(RegionRoot*,RegionRoot*,RegionRect,const void*,int);
 int (*readRGBA)(RegionRoot*,RegionRoot*,void*,int);
 int (*surfaceGetDesc)(RegionRoot*,RegionRoot*,RegionDesc*);
 float (*sumVertices)(RegionRoot*,const RegionVertex*);
 int (*sumInts)(RegionRoot*,const int*);
 unsigned depth; int mode,queries,calls; bool closing;
};
extern const XL6_ForeignViewDescriptorV1 regionPolicy;
int region_root_create(RegionRoot**);
void region_root_destroy(RegionRoot*);
int region_root_mode(RegionRoot*,int);
int region_root_queries(RegionRoot*);
int region_root_calls(RegionRoot*);
int region_free_read(int,int,const void*,int);
int region_wide_read(uint64_t,uint64_t,const void*,uint64_t);
void region_reset_failure(void);
void region_fail_after(int);
RegionQuad region_vertices(void);
RegionInts region_ints(void);
float region_sum_vertices(const RegionVertex*);
int region_callback_read(int,int,const void*,int,int(*)(const uint8_t*,size_t,void*),void*);
#endif
