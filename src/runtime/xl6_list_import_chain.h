#ifndef XL6_LIST_IMPORT_CHAIN_H
#define XL6_LIST_IMPORT_CHAIN_H
#include "xl6_collection_import_chain.h"
/* Optional typed List template/append adapter; does not link a graph copier,
 * duplicate memo, wire container implementation or per-element Cgen body. */
XL6_TEXT_API const XL6_CollectionImportProviderV1* xl6ListImportProviderV1(void);
#endif
