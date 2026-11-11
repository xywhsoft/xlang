#ifndef XL6_TEST_FOREIGN_CSTRING_OUTPUT_H
#define XL6_TEST_FOREIGN_CSTRING_OUTPUT_H
#include "test/cases/foreign_managed_view.h"
int cstr_seed(ManagedRoot*);
int cstr_read(ManagedRoot*,int,const char**);
int cstr_optional(ManagedRoot*,int,const char**);
int cstr_get(ManagedRoot*,const char**);
int cstr_pair(ManagedRoot*,const char**,int*);
int cstr_pair_text(ManagedRoot*,const char**,const char**);
int cstr_first(const char**,int);
void cstr_void(const char**,int);
void cstr_change(ManagedRoot*);
void cstr_fail_after(int);
void cstr_counter_end(int);
void cstr_heap_end(void);
int cstr_span(ManagedRoot*,const uint8_t*,int,const char**);
typedef struct CstrReader CstrReader;
struct CstrReader {int (*read)(CstrReader*,ManagedRoot*,const char**);};
CstrReader* cstr_reader(void);
#endif
