#ifndef XLIR_MODULE_RETIREMENT_V4_HOOKS_H
#define XLIR_MODULE_RETIREMENT_V4_HOOKS_H
typedef struct RetirementHooks {
    int status, failInit, ownLease;
    unsigned initializations, finalizers, steps, legacyCalls, completions;
} RetirementHooks;
#endif
