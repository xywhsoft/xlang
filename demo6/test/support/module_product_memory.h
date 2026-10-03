#ifndef XL6_TEST_MODULE_PRODUCT_MEMORY_H
#define XL6_TEST_MODULE_PRODUCT_MEMORY_H
#include <stdbool.h>
#include <stddef.h>
void xl6ProductMemoryBegin(size_t point);
bool xl6ProductMemoryEnd(void);
void xl6ProductMemoryVerify(void);
#endif
