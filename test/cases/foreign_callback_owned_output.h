#ifndef XL_TEST_FOREIGN_CALLBACK_OWNED_OUTPUT_H
#define XL_TEST_FOREIGN_CALLBACK_OWNED_OUTPUT_H

#include <xrt_decl.h>

typedef struct CallbackOwnedOutput CallbackOwnedOutput;
typedef int (*CallbackOwnedOutputProc)(CallbackOwnedOutput**, CallbackOwnedOutput**, int, void*);
typedef struct CallbackOutputBackend {
    void* user;
    int (*write)(const struct CallbackOutputBackend*, CallbackOwnedOutput**, CallbackOwnedOutput**, int);
} CallbackOutputBackend;
typedef struct CallbackOutputOwner {
    xrtownershipref reference;
    bool (*Hold)(const void*);
    bool (*Drop)(const void*);
} CallbackOutputOwner;

int xl_test_owned_output_invoke(CallbackOwnedOutputProc, void*, int);
int xl_test_owned_output_retained(const CallbackOutputBackend*, const CallbackOutputOwner*, int);
int xl_test_owned_output_warm(CallbackOwnedOutputProc, void*);
int xl_test_owned_output_store(CallbackOwnedOutput**, int);
void xl_test_owned_output_drop(CallbackOwnedOutput*);

#endif
