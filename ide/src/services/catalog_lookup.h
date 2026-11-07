#ifndef XIDE_CATALOG_LOOKUP_H
#define XIDE_CATALOG_LOOKUP_H
#include "services/command_catalog.h"
#include <stdint.h>
typedef struct xide_catalog_lookup xide_catalog_lookup;
xide_catalog_lookup* xideCatalogLookupCreate(const xide_command_catalog*);
void xideCatalogLookupDestroy(xide_catalog_lookup*);
/* Sorted 32-bit indices. No allocation or catalog-wide scan during a query.
 * Results are explicitly spelling candidates, not semantic bindings. */
size_t xideCatalogLookupName(const xide_catalog_lookup*,const char*,const char*,int,uint32_t*,size_t,size_t*);
const xide_command* xideCatalogLookupId(const xide_catalog_lookup*,const char*);
size_t xideCatalogLookupBytes(const xide_catalog_lookup*);
#endif
