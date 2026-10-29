#ifndef XL6_LIST_VALUE_WRITE_H
#define XL6_LIST_VALUE_WRITE_H
#include <stdbool.h>
#include <stdint.h>
typedef struct xvalue xvalue;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_LIST_VALUE_WRITE_API __declspec(dllexport)
#else
#define XL6_LIST_VALUE_WRITE_API
#endif
/* Borrowed List/full signed key, input consumed once on ALL paths. Native
 * resident admission precedes commit. Legacy IntMap replacement pins receiver
 * and old child so their destruction runs outside its BUSY mutation fence.
 * First checked outcome is latched before any retirement callback. */
XL6_LIST_VALUE_WRITE_API bool xl6ListValueSetTakeV1(xvalue*,int64_t,xvalue*);
#endif
