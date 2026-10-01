#ifndef XL6_TEXT_VALUE_H
#define XL6_TEXT_VALUE_H
#include <stdint.h>
#if !defined(XRT_SINGLE_HEADER) && !defined(XRUNTIME_DECLARATIONS_H)
#include "include/xruntime_decl.h"
#endif
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_TEXT_API __declspec(dllexport)
#else
#define XL6_TEXT_API
#endif

/* Inputs borrow exact UTF-8 views for the call. Every successful result owns
 * an independent XRT allocation, including empty strings; free only Data.
 * Failure returns {NULL,0} and sets the XRT thread error. No strlen/heap-size
 * inference or library implementation belongs in generated C. */
XL6_TEXT_API xstrview xl6TextLowerV1(xstrview text, int64_t size);
XL6_TEXT_API xstrview xl6TextUpperV1(xstrview text, int64_t size);
XL6_TEXT_API xstrview xl6TextTrimV1(xstrview text, xstrview chars, int64_t size, int64_t charsSize);
XL6_TEXT_API xstrview xl6TextLTrimV1(xstrview text, xstrview chars, int64_t size, int64_t charsSize);
XL6_TEXT_API xstrview xl6TextRTrimV1(xstrview text, xstrview chars, int64_t size, int64_t charsSize);
XL6_TEXT_API xstrview xl6TextFilterV1(xstrview text, xstrview chars, int64_t size, int64_t charsSize);
XL6_TEXT_API xstrview xl6TextReplaceV1(xstrview text, xstrview part, xstrview replacement,
    int64_t size, int64_t partSize, int64_t replacementSize);
XL6_TEXT_API xstrview xl6TextSliceV1(xstrview text, int64_t start, int64_t count, int64_t size);
XL6_TEXT_API xstrview xl6TextLeftV1(xstrview text, int64_t count, int64_t size);
XL6_TEXT_API xstrview xl6TextRightV1(xstrview text, int64_t count, int64_t size);
XL6_TEXT_API xstrview xl6TextRepeatV1(xstrview text, int64_t count, int64_t size);
XL6_TEXT_API xstrview xl6TextReverseV1(xstrview text, int64_t size);
XL6_TEXT_API xstrview xl6TextRemoveAtV1(xstrview text, int64_t start, int64_t count, int64_t size);
XL6_TEXT_API xstrview xl6TextInsertV1(xstrview text, int64_t index, xstrview part,
    int64_t size, int64_t partSize);
#endif
