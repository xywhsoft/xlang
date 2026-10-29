#ifndef XL6_BUILD_BACKEND_H
#define XL6_BUILD_BACKEND_H

typedef enum XL6_BuildBackendKind {
    XL6_BUILD_BACKEND_TCC
} XL6_BuildBackendKind;

typedef struct XL6_BuildBackend {
    XL6_BuildBackendKind kind;
    const char* name;
    const char* ccPath;
    const char* commandPrefix;
} XL6_BuildBackend;

void xl6BuildBackendInitDefault(XL6_BuildBackend* backend);
const char* xl6BuildBackendName(const XL6_BuildBackend* backend);
const char* xl6BuildBackendCcPath(const XL6_BuildBackend* backend);
const char* xl6BuildBackendCommandPrefix(const XL6_BuildBackend* backend);

#endif
