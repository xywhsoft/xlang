#ifndef XL6_BUILD_INPUTS_H
#define XL6_BUILD_INPUTS_H
#include "src/compile/xl6_source_report.h"

#define XL6_BUILD_INPUT_FILES 1024u
#define XL6_BUILD_INPUT_FILE_BYTES (64u * 1024u * 1024u)
#define XL6_BUILD_INPUT_TOTAL_BYTES (256u * 1024u * 1024u)
#define XL6_BUILD_INPUT_PROBES 32768u
#define XL6_BUILD_INPUT_PROBE_BYTES (8u * 1024u * 1024u)

typedef struct XL6_BuildInput {
    char* path;
    unsigned char* data;
    size_t size;
    xfileinfo info;
    unsigned char sha256[32];
} XL6_BuildInput;

typedef struct XL6_BuildInputs {
    XL6_SourceReport sources;
    XL6_BuildInput* files;
    size_t count, capacity, bytes;
    char** missing;
    size_t missingCount, missingCapacity, missingBytes;
    int complete;
    char error[256];
    XL6_BuildInput artifact;
    int built;
} XL6_BuildInputs;

void xl6BuildInputsInit(XL6_BuildInputs* report);
void xl6BuildInputsFree(XL6_BuildInputs* report);
/* Same lexical path reuses first bytes. Missing native candidates are recorded
 * separately from xlang's regular-file resolution predicate. */
int xl6BuildInputsRead(void* report, const char* path,
    const unsigned char** data, size_t* size);
int xl6BuildInputsValidate(XL6_BuildInputs* report);
int xl6BuildInputsOutputValid(XL6_BuildInputs* report, const char* path);
int xl6BuildInputsBindArtifact(XL6_BuildInputs* report, const char* path);
int xl6BuildInputsWrite(XL6_BuildInputs* report, const char* artifact, const char* receipt);
/* Borrowed activation, protected by the compiler API lock just like TCC's
 * disk-reader callback and the process work directory. Callers of activation
 * and the resource readers below must hold that lock and restore the previous
 * binding on all exits. Ordinary compilation retains its existing reads. */
XL6_BuildInputs* xl6BuildInputsActivate(XL6_BuildInputs* report);
char* xl6BuildInputReadText(const char* path, size_t* size);
void* xl6BuildInputReadAll(const char* path, size_t* size);
int xl6BuildInputOutputValid(const char* path);
int xl6BuildInputExists(const char* path);

/* Internal CLI/compiler contract. No compiler declaration is added to the
 * runtime DLL's public ABI. */
struct XLC_Context;
int xlcBeginInputReport(struct XLC_Context* ctx);
char* xlcReadProjectInput(struct XLC_Context* ctx, const char* path, size_t* size);
int xlcWriteInputReport(struct XLC_Context* ctx, const char* artifact, const char* receipt);
#endif
