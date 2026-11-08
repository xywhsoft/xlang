#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static int allocations, releases, failAllocation;
static void* meshAllocate(size_t bytes) { ++allocations; return failAllocation ? NULL : malloc(bytes); }
static void meshRelease(void* data) { ++releases; free(data); }
#define malloc meshAllocate
#define free meshRelease
#include "../src/platform/dpi_proxy.c"
#undef malloc
#undef free

static const xui_mesh_vertex_t* original;
static const uint32_t* originalIndices;
static int expectedCount, calls;
static float expectedScale;
static int receiveMesh(xui_proxy proxy, xui_draw_context context, const xui_mesh_vertex_t* vertices,
                       int count, const uint32_t* indices, int indexCount, uint32_t flags)
{
    (void)proxy; (void)context;
    if(count!=expectedCount || indices!=originalIndices || indexCount!=3 || flags!=7) return -100;
    for(int i=0;i<count;++i) {
        xui_mesh_vertex_t expected=original[i];
        expected.fX*=expectedScale; expected.fY*=expectedScale;
        if(memcmp(&vertices[i],&expected,sizeof(expected))) return -101;
    }
    ++calls;
    return 123;
}
#define CHECK(e) do { if(!(e)) { fprintf(stderr,"dpi_mesh_test:%d: %s\n",__LINE__,#e); return 1; } } while(0)
int main(void)
{
    xui_mesh_vertex_t vertices[17]={0}, saved[17]; uint32_t indices[3]={0,1,2};
    for(int i=0;i<17;++i) { vertices[i].fX=i+0.25f; vertices[i].fY=i+0.5f; vertices[i].iColor=0x6ed0a040u; }
    memcpy(saved,vertices,sizeof(saved)); original=vertices; originalIndices=indices;
    xide_dpi_proxy owner={0}; owner.raw.drawMeshTriangles=receiveMesh;
    xui_proxy_t proxy={0}; proxy.pUser=&owner;
    dpi_draw context={0}; context.magic=DRAW_MAGIC;
    for(int step=0;step<5;++step) {
        expectedScale=context.scale=1+step*0.25f;
        for(int size=0;size<4;++size) {
            int counts[]={0,4,16,17}; expectedCount=counts[size]; allocations=releases=calls=0;
            CHECK(drawMeshTriangles(&proxy,(xui_draw_context)&context,vertices,expectedCount,indices,3,7)==123);
            CHECK(calls==1 && allocations==(expectedCount>16) && releases==allocations);
            CHECK(!memcmp(saved,vertices,sizeof(saved)));
        }
    }
    failAllocation=1; allocations=releases=calls=0;
    CHECK(drawMeshTriangles(&proxy,NULL,vertices,17,indices,3,7)==XUI_ERROR_OUT_OF_MEMORY);
    CHECK(allocations==1 && releases==0 && calls==0);
    CHECK(drawMeshTriangles(&proxy,NULL,vertices,-1,indices,3,7)==XUI_ERROR_INVALID_ARGUMENT);
    CHECK(drawMeshTriangles(&proxy,NULL,NULL,4,indices,3,7)==XUI_ERROR_INVALID_ARGUMENT);
    CHECK(drawMeshTriangles(NULL,NULL,vertices,4,indices,3,7)==XUI_ERROR_INVALID_ARGUMENT);
    puts("dpi_mesh_test passed: 100-200% scale, 0/4/16/17 vertices, zero small-mesh heap allocations, large-mesh allocation/free, unchanged colors/indices/input, error propagation and OOM");
    return 0;
}
