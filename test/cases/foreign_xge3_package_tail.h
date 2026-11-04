#ifndef FOREIGN_XGE3_PACKAGE_TAIL_H
#define FOREIGN_XGE3_PACKAGE_TAIL_H
#include <xrt_decl.h>
#if defined(_WIN32)
#if defined(TAIL_BUILD_DLL)
#define TAIL_API __declspec(dllexport)
#else
#define TAIL_API __declspec(dllimport)
#endif
#else
#define TAIL_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct TailEvent {int value;} TailEvent;
typedef void (*TailHandler)(const TailEvent*,void*);
typedef struct TailOwner {xrtownershipref reference;bool(*Hold)(const void*);bool(*Drop)(const void*);} TailOwner;
typedef struct TailControl {volatile unsigned phase,release;} TailControl;
TAIL_API int tail_install(TailHandler,void*,const TailOwner*);
TAIL_API int tail_dispatch(TailControl*);
/* This function belongs to the resident test host, not the publisher DLL. */
void tail_request_close(void);
#ifdef __cplusplus
}
#endif
#endif
