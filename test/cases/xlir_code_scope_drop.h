#ifndef XLIR_CODE_SCOPE_DROP_H
#define XLIR_CODE_SCOPE_DROP_H
typedef struct ScopeDropHooks {
    void (*during)(void*);
    void* context;
    unsigned returned;
} ScopeDropHooks;
#endif
