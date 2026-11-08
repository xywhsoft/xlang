#ifndef XIDE_CATALOG_JOB_H
#define XIDE_CATALOG_JOB_H
#include "services/command_catalog.h"
#include "services/catalog_lookup.h"
#include "xui.h"
typedef struct xide_catalog_job xide_catalog_job;
typedef struct xide_catalog_result {
    xide_command_catalog catalog; xide_catalog_lookup* lookup;
    char** details; xui_tree_view_node_t* nodes; int nodeCount; size_t visible;
    char error[600]; double loadMs;
} xide_catalog_result;
int xideCatalogTreePrepare(const xide_command_catalog*,const char*,xui_tree_view_node_t**,int*,size_t*);
xide_catalog_job* xideCatalogJobStart(const char*);
/* 0 pending, 1 completed. Transfers the immutable result once. */
int xideCatalogJobTake(xide_catalog_job*,xide_catalog_result*);
void xideCatalogJobCancel(xide_catalog_job*);
void xideCatalogResultClear(xide_catalog_result*);
#endif
