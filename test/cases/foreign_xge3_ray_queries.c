#include "foreign_xge3_ray_queries.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int fixtureRayInput(int which,xge3d_ray_t* ray,xge3d_aabb_t* box,
    xge3d_sphere_t* sphere,xge3d_vec3_t triangle[3],float* maximum)
{
    assert(which>=0 && which<26);
    *ray=(xge3d_ray_t){{0,0,3},{0,0,-2}};
    *box=(xge3d_aabb_t){{-1,-1,-1},{1,1,1}};
    *sphere=(xge3d_sphere_t){{0,0,0},1};
    triangle[0]=(xge3d_vec3_t){-1,-1,0};
    triangle[1]=(xge3d_vec3_t){1,-1,0};
    triangle[2]=(xge3d_vec3_t){0,1,0};
    *maximum=10;
    switch(which){
    case 1:*maximum=1;break;
    case 2:*maximum=2;break;
    case 3:ray->origin.x=4;break;
    case 4:ray->origin.z=0;break;
    case 5:ray->direction.z=0;break;
    case 6:*maximum=0;break;
    case 7:*maximum=-1;break;
    case 8:*maximum=NAN;break;
    case 9:*maximum=INFINITY;break;
    case 10:ray->origin.x=NAN;break;
    case 11:ray->direction.z=INFINITY;break;
    case 12:box->min.x=2;break;
    case 13:box->max.x=NAN;break;
    case 14:sphere->radius=-1;break;
    case 15:sphere->center.x=INFINITY;break;
    case 16:triangle[1].x=NAN;break;
    case 17:triangle[2]=triangle[0];break;
    case 18:ray->direction.z=2;break;
    case 19:{xge3d_vec3_t a=triangle[0];triangle[0]=triangle[1];triangle[1]=a;break;}
    case 20:ray->origin.x=1;break;
    case 21:ray->origin.z=1;break;
    case 22:sphere->radius=0;break;
    case 23:*box=(xge3d_aabb_t){{0,0,0},{0,0,0}};break;
    case 24:*maximum=3;break;
    case 25:sphere->radius=NAN;break;
    default:break;
    }
    return XGE_OK;
}

static void query(int which,int status[3],float distance[3],xge3d_hit_t* hit)
{
    xge3d_ray_t ray;xge3d_aabb_t box;xge3d_sphere_t sphere;
    xge3d_vec3_t triangle[3];float maximum;
    assert(fixtureRayInput(which,&ray,&box,&sphere,triangle,&maximum)==XGE_OK);
    status[0]=xge3dRayAabb(&ray,&box,maximum,&distance[0]);
    status[1]=xge3dRaySphere(&ray,&sphere,maximum,&distance[1]);
    status[2]=xge3dRayTriangle(&ray,triangle,maximum,hit);distance[2]=hit->distance;
}

static void expected(int which,const int status[3],const float distance[3])
{
    int codes[3]={XGE_OK,XGE_OK,XGE_OK};float distances[3]={2,2,3};
    switch(which){
    case 1:case 3:case 18:
        for(int i=0;i<3;++i){codes[i]=XGE_ERROR_NOT_FOUND;distances[i]=0;}break;
    case 2:codes[2]=XGE_ERROR_NOT_FOUND;distances[2]=0;break;
    case 4:distances[0]=distances[1]=distances[2]=0;break;
    case 5:case 6:case 7:case 8:case 9:case 10:case 11:
        for(int i=0;i<3;++i){codes[i]=XGE_ERROR_INVALID_ARGUMENT;distances[i]=0;}break;
    case 12:case 13:codes[0]=XGE_ERROR_INVALID_ARGUMENT;distances[0]=0;break;
    case 14:case 15:case 25:codes[1]=XGE_ERROR_INVALID_ARGUMENT;distances[1]=0;break;
    case 16:codes[2]=XGE_ERROR_INVALID_ARGUMENT;distances[2]=0;break;
    case 17:codes[2]=XGE_ERROR_NOT_FOUND;distances[2]=0;break;
    case 20:distances[1]=3;codes[2]=XGE_ERROR_NOT_FOUND;distances[2]=0;break;
    case 21:distances[0]=distances[1]=0;distances[2]=1;break;
    case 22:distances[1]=3;break;
    case 23:distances[0]=3;break;
    default:break;
    }
    for(int i=0;i<3;++i){assert(status[i]==codes[i]);assert(distance[i]==distances[i]);}
}

void fixtureRayCompare(int which,int box_status,float box_distance,
    int sphere_status,float sphere_distance,int triangle_status,xge3d_hit_t hit)
{
    int status[3];float distance[3];xge3d_hit_t native;
    query(which,status,distance,&native);expected(which,status,distance);
    assert(box_status==status[0] && box_distance==distance[0]);
    assert(sphere_status==status[1] && sphere_distance==distance[1]);
    assert(triangle_status==status[2] && hit.distance==native.distance);
    assert(hit.node.slot==native.node.slot && hit.node.scene==native.node.scene);
    assert(hit.model_root.slot==native.model_root.slot && hit.model_root.scene==native.model_root.scene);
    assert(hit.triangle==native.triangle);
    assert(hit.position.x==native.position.x && hit.position.y==native.position.y && hit.position.z==native.position.z);
    assert(hit.normal.x==native.normal.x && hit.normal.y==native.normal.y && hit.normal.z==native.normal.z);
}

void fixtureRayNativeOracle(void)
{
    for(int i=0;i<26;++i){int status[3];float distance[3];xge3d_hit_t hit;
        query(i,status,distance,&hit);expected(i,status,distance);
        if(status[2]!=XGE_OK){xge3d_hit_t cleared={0};assert(!memcmp(&hit,&cleared,sizeof hit));}
    }
    puts("XGE3D_RAY_NATIVE_OK");
}

static xmemdebugsnapshot before;
void fixtureRayHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);
}
void fixtureRayHeapEnd(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.AllocCount==before.AllocCount && after.FreeCount==before.FreeCount &&
        after.ReallocCount==before.ReallocCount && after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes);
    printf("Ray queries %s: calls=3000 alloc=0 free=0 realloc=0\n",native ? "native":"language");
}
void fixtureRayNativeHot(void)
{
    xge3d_ray_t ray;xge3d_aabb_t box;xge3d_sphere_t sphere;
    xge3d_vec3_t triangle[3];float maximum;
    assert(!fixtureRayInput(0,&ray,&box,&sphere,triangle,&maximum));
    for(int i=0;i<1000;++i){float a=0,b=0;xge3d_hit_t hit;
        assert(!xge3dRayAabb(&ray,&box,maximum,&a) && a==2);
        assert(!xge3dRaySphere(&ray,&sphere,maximum,&b) && b==2);
        assert(!xge3dRayTriangle(&ray,triangle,maximum,&hit) && hit.distance==3);
    }
}
