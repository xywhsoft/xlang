#ifndef XL6_DICT_IMPORT_CHAIN_H
#define XL6_DICT_IMPORT_CHAIN_H
#include "xl6_collection_import_chain.h"
/* Resident exact-text-key adapter for the common import/restore transaction.
 * Genuine native Dict or explicit wire OBJECT input; no mirror map, registry,
 * source callback, slot borrow or native authority inferred from TypeId. */
XL6_TEXT_API const XL6_CollectionImportProviderV1* xl6DictImportProviderV1(void);
#endif
