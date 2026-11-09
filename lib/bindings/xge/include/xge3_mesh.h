#ifndef XL6_XGE3_MESH_H
#define XL6_XGE3_MESH_H
#include "xge3_render_resources.h"
XL6_XGE_RECORD_POLICY(xl6Xge3MeshPolicy,"xge.mesh",xge_mesh_t,xgeMeshFree)
#define xl6Xge3MeshPolicy (*xl6Xge3MeshPolicyGet())
#endif
