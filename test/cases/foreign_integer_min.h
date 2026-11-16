#ifndef FOREIGN_INTEGER_MIN_TEST_H
#define FOREIGN_INTEGER_MIN_TEST_H
#include <stdint.h>
typedef struct integer_min_box { int value; } integer_min_box;
int integer_min_count(int value);
int integer_min_find(int value);
int integer_min_positive(int value);
long long integer_min_wide(long long value);
int integer_min_failure(int mode);
integer_min_box* integer_min_box_get(void);
int integer_min_box_lookup(integer_min_box*,int value);
int integer_min_box_count(integer_min_box*);
void integer_min_begin(void);
int integer_min_calls(void);
int integer_min_allocations(void);
void integer_min_reset(void);
#endif
