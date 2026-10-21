#ifndef XLANG_IDE_TOOLCHAIN_H
#define XLANG_IDE_TOOLCHAIN_H
#include "platform/file_rename.h"
#include <stdint.h>
typedef struct xide_toolchains xide_toolchains;
typedef struct xide_toolchain_request xide_toolchain_request;
enum xide_toolchain_command { XIDE_TOOL_BUILD=1u, XIDE_TOOL_RUN=2u, XIDE_TOOL_CHECK=4u, XIDE_TOOL_LEX=8u, XIDE_TOOL_PARSE=16u, XIDE_TOOL_IR=32u, XIDE_TOOL_CGEN=64u, XIDE_TOOL_ABI=128u, XIDE_TOOL_IMPDEF=256u, XIDE_TOOL_CBRIDGE=512u };
enum xide_toolchain_option { XIDE_TOOL_OUTPUT=1u, XIDE_TOOL_INCLUDE=2u, XIDE_TOOL_LIBRARY=4u, XIDE_TOOL_LINK=8u, XIDE_TOOL_RESOURCE=16u, XIDE_TOOL_SHARED=32u, XIDE_TOOL_SAVE_TEMPS=64u, XIDE_TOOL_HEADER=128u, XIDE_TOOL_BACKEND=256u, XIDE_TOOL_CHECK_JSON=512u, XIDE_TOOL_ABI_JSON=1024u, XIDE_TOOL_CHECK_OVERLAY=2048u, XIDE_TOOL_CHECK_SOURCE_REPORT=4096u, XIDE_TOOL_CHECK_SEMANTIC_INDEX=8192u, XIDE_TOOL_CHECK_COMPLETION=16384u, XIDE_TOOL_CBRIDGE_JSON=32768u, XIDE_TOOL_CBRIDGE_ENVIRONMENT=65536u, XIDE_TOOL_BUILD_INPUT_REPORT=131072u };
typedef struct xide_toolchain_stamp { xide_file_snapshot files[3]; int present[3]; } xide_toolchain_stamp;
typedef struct xide_toolchain_profile {
 char *compiler,*directory,*help;
 xide_toolchain_stamp stamp;
 uint32_t commands,options;
 uint64_t observedAt,durationMs;
 int cached;
 char fileVersion[64];
} xide_toolchain_profile;
enum xide_toolchain_request_state { XIDE_TOOL_QUERY_RUNNING, XIDE_TOOL_QUERY_READY, XIDE_TOOL_QUERY_FAILED, XIDE_TOOL_QUERY_CANCELLED };
/* Profile outputs must be zero-initialized or previously owned by this API. */
void xideToolchainProfileClear(xide_toolchain_profile* profile);
int xideToolchainProfileCopy(xide_toolchain_profile* out,const xide_toolchain_profile* profile);
/* Exact command lines and option tokens only; help is an observation, not a
   claim that check is project-wide, or that LSP/DAP/semantic APIs exist. */
int xideToolchainParseHelp(const char* help,size_t length,uint32_t* commands,uint32_t* options,xide_model_error* error);
/* Worker-only I/O. Missing sibling xlc.dll/xl.dll are explicit stamp states. */
int xideToolchainCapture(const char* compiler,xide_toolchain_stamp* stamp,xide_model_error* error);
int xideToolchainStampEqual(const xide_toolchain_stamp* a,const xide_toolchain_stamp* b);
int xideToolchainValidate(const char* compiler,const xide_toolchain_stamp* stamp,xide_model_error* error);
xide_toolchains* xideToolchainsCreate(void);
void xideToolchainsDestroy(xide_toolchains* cache);
/* Called after an actual successful --help process, compares post-query files
   with before, copies all data, and records the observation in the cache. */
int xideToolchainObserve(xide_toolchains* cache,const char* compiler,const char* directory,const xide_toolchain_stamp* before,const char* help,size_t length,xide_toolchain_profile* out,xide_model_error* error);
xide_toolchain_request* xideToolchainRequest(xide_toolchains* cache,const char* compiler,const char* directory,int force,xide_model_error* error);
/* Owner thread polls; copied output strings use malloc/free. Cache must outlive
   requests. Cancellation never calls UI and close can retain a pending job. */
enum xide_toolchain_request_state xideToolchainRequestState(xide_toolchain_request* request,char* message,size_t capacity);
int xideToolchainRequestResult(xide_toolchain_request* request,xide_toolchain_profile* out);
void xideToolchainRequestCancel(xide_toolchain_request* request);
void xideToolchainRequestDestroy(xide_toolchain_request* request);
#endif
