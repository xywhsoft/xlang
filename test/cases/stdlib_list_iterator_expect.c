/* Exact C declarations for the unchanged legacy extern expectations. */
#include <assert.h>
#include <stdint.h>
#include <string.h>
void expect_i64(int64_t actual,int64_t expected) { assert(actual==expected); }
void expect_str(const char* actual,const char* expected)
{ assert(actual && expected && !strcmp(actual,expected)); }
