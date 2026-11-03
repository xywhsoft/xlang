#ifndef FOREIGN_BOUND_VIEW_H
#define FOREIGN_BOUND_VIEW_H
#include <stdint.h>
typedef struct bound_view { int64_t value; } bound_view;
bound_view* bound_view_at(int);
bound_view* bound_view_child(int);
int64_t bound_view_add(bound_view*,int64_t);
int64_t bound_view_mul(bound_view*,int64_t);
void bound_view_store(bound_view*,int64_t);
void bound_view_check_begin(void);
int bound_view_check_unchanged(void);
int bound_view_calls(void);
void bound_view_binding_cost(void);
void bound_view_fail_after(int);
void bound_view_mark_balance(void);
int bound_view_balanced(void);
#endif
