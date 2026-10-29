#include "xl.h"
#include "xrt.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define XL6_OWNERSHIP_DOMAIN_ENTRY(Producer, Component, Getter) #Getter,
static const char* const domain_names[] = {
#include "src/runtime/xl6_ownership_domains.inc"
};
#undef XL6_OWNERSHIP_DOMAIN_ENTRY

static void print_error(const char* stage)
{
    const xerror* error = xrtGetError();
    fprintf(stderr, "%s: kind=%d domain=%s operation=%s message=%s\n",
        stage, error ? (int)xrtErrorKind(error) : -1,
        error ? xrtErrorDomain(error) : "(none)",
        error ? xrtErrorOperation(error) : "(none)",
        error ? xrtErrorMessage(error) : "(none)");
}

int main(void)
{
    XL_ContextOptions options = xlDefaultContextOptions();
    XL_Context* ctx = xlCreateContext(&options);
    if (!ctx) {
        print_error("xlCreateContext");
        return 1;
    }
    const size_t count = sizeof(domain_names) / sizeof(domain_names[0]);
    const XL6_OwnershipDomainV1* domains[sizeof(domain_names) / sizeof(domain_names[0])];
    HMODULE library = GetModuleHandleA("xlc.dll");
    size_t loaded = 0;
    size_t accepted = 0;
    if (!library) {
        fprintf(stderr, "xlc.dll is not loaded\n");
        xlDestroyContext(ctx);
        return 1;
    }
    for (size_t i = 0; i < count; ++i) {
        FARPROC address = GetProcAddress(library, domain_names[i]);
        const XL6_OwnershipDomainV1* (*getter)(void);
        _Static_assert(sizeof(address) == sizeof(getter), "function pointer size");
        if (!address) {
            fprintf(stderr, "getter not exported: %zu/%zu %s\n", i + 1,
                count, domain_names[i]);
            continue;
        }
        memcpy(&getter, &address, sizeof(getter));
        domains[loaded++] = getter();
        xrtClearError();
        XL6_OwnershipCatalog* catalog = xl6OwnershipCatalogCreateV1(domains, loaded);
        if (!catalog) {
            fprintf(stderr, "first failing domain: %zu/%zu %s\n", i + 1,
                count, domain_names[i]);
            print_error("xl6OwnershipCatalogCreateV1");
            break;
        }
        xl6OwnershipCatalogDestroyV1(catalog);
        accepted = loaded;
    }
    fprintf(stderr, "exported domains accepted: %zu/%zu\n", accepted, count);
    xrtClearError();
    int ok = xlRunScriptString(ctx, "println(\"hello\")\n");
    if (!ok) {
        fprintf(stderr, "xlRunScriptString: %s\n", xlGetLastError(ctx));
        print_error("xlRunScriptString");
    }
    xlDestroyContext(ctx);
    return ok ? 0 : 1;
}
