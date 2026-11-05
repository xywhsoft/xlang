#ifndef XL6_XGE3_BINDING_MANAGERS_H
#define XL6_XGE3_BINDING_MANAGERS_H
int binding_manager_budget_begin(void);
int binding_manager_budget_end(void);
void binding_manager_callback_entered(void);
int binding_manager_callback_count(void);
#endif
