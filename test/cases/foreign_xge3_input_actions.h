#ifndef XL6_TEST_XGE3_ACTIONS_H
#define XL6_TEST_XGE3_ACTIONS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void actions_test_begin(void);
int actions_test_balanced(void);
uint64_t actions_test_allocations(void);
int actions_test_fail(int count);
void actions_test_clear(void);
#ifdef __cplusplus
}
#endif
#endif
