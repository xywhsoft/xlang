#ifndef XL6_COLLECTION_COUNT_ALL_H
#define XL6_COLLECTION_COUNT_ALL_H
#include "xl6_sequence.h"
/* Open language var count. Closed Array/List/Set calls retain their separately
 * selected providers, so a leaf Array never links List/Set just for count. */
XL6_TEXT_API int64_t xl6CollectionCountAllV1(const xvalue*);
XL6_TEXT_API bool xl6CollectionCountAllIfPresentV1(const xvalue*,int64_t*);
XL6_TEXT_API xvalue* xl6CollectionCountAllOrNullV1(const xvalue*);
#endif
