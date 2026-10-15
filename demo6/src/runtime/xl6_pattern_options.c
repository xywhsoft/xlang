#include "src/runtime/xl6_pattern_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

void xl6PatternOptionsDestroyV1(void* handle)
{
    XL6_PatternOptions* options=handle;
    if (options) {xrtFree(options->separators);xrtFree(options);}
}
void* xl6PatternOptionsCreateV1(xstrview separators,int64_t bytes,int64_t patterns,
    int64_t captures,int64_t states,int64_t compiled,int64_t size)
{
    if (!xl6TextSelect(&separators,size)) return NULL;
    if (bytes<=0 || patterns<=0 || captures<=0 || states<=0 || compiled<=0 ||
        (uint64_t)bytes>SIZE_MAX || (uint64_t)patterns>SIZE_MAX ||
        (uint64_t)captures>SIZE_MAX || (uint64_t)states>SIZE_MAX || (uint64_t)compiled>SIZE_MAX) {
        xrtSetErrorKind(XERR_RANGE);return NULL;
    }
    /* XRT separators are a byte set, not Unicode characters or substrings.
     * Restrict the string facade to ASCII bytes so captures stay scalar-boundary
     * aligned; NUL and an empty separator set are both valid. */
    for (size_t i=0;i<separators.Size;++i) if ((unsigned char)separators.Data[i]>=128) {
        xrtSetErrorKind(XERR_VALUE);return NULL;
    }
    xpatternconfig config;xrtPatternConfigInit(&config);
    config.Separators=separators;config.MaxPatternBytes=(size_t)bytes;
    config.MaxPatterns=(size_t)patterns;config.MaxCaptures=(size_t)captures;
    config.MaxStates=(size_t)states;config.MaxCompiledBytes=(size_t)compiled;
    /* Ask the canonical compiler to validate all reserved/range/minimum-budget
     * rules; don't duplicate its private option parser in the language adapter. */
    xpattern* validation=xrtPatternCompileManyConfig(NULL,0,&config);
    if (!validation) return NULL;
    xrtPatternRelease(validation);
    XL6_PatternOptions* options=xrtCalloc(1,sizeof(*options));
    if (!options) return NULL;
    options->separators=xrtStrDupView(separators);
    if (!options->separators) {xl6PatternOptionsDestroyV1(options);return NULL;}
    options->config=config;options->config.Separators=(xstrview){options->separators,separators.Size};
    return options;
}
xstrview xl6PatternOptionsSeparatorsV1(void* handle)
{
    if (!handle) {xrtSetErrorKind(XERR_ARGUMENT);return xl6TextFailure;}
    return xl6TextCopy(((XL6_PatternOptions*)handle)->config.Separators);
}
#define OPTION(Name,Field) \
int64_t xl6PatternOptions##Name##V1(void* handle) { \
    if (!handle) {xrtSetErrorKind(XERR_ARGUMENT);return 0;} \
    return (int64_t)((XL6_PatternOptions*)handle)->config.Field; }
OPTION(Bytes,MaxPatternBytes)
OPTION(Patterns,MaxPatterns)
OPTION(Captures,MaxCaptures)
OPTION(States,MaxStates)
OPTION(Compiled,MaxCompiledBytes)
#undef OPTION

/* Resident policy: terminal Drop runs after the phased handle is detached. */
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6PatternOptionsHandlePolicyV1Get,xl6PatternOptionsDestroyV1)
