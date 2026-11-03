#ifndef XL6_TEST_FOREIGN_OUTPUT_VALUES_H
#define XL6_TEST_FOREIGN_OUTPUT_VALUES_H
#include "foreign_status.h"
#include "foreign_expanded_calls.h"
int output_first(int*,int);
int output_record(int,SpanPoint*);
int output_span(const uint8_t*,int,int*);
int output_root(ManagedRoot*,int*);
int output_root_read(ManagedRoot*,int*);
int output_root_span(ManagedRoot*,const uint8_t*,int,int*);
#endif
