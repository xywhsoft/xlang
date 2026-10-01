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

/* Inputs borrow exact views for the call; textual operations validate UTF-8.
 * Every successful string result owns an independent XRT allocation, even
 * when empty; free only Data. Value/array/bytes results own one xvalue reference.
 * On failure string returns {NULL,0}, managed values return NULL, and scalar
 * operations return their documented sentinel, with an XRT thread error.
 * No strlen/heap-size inference or library implementation belongs in Cgen. */
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
XL6_TEXT_API xstrview xl6TextRemoveV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize);
XL6_TEXT_API xstrview xl6TextPadLeftV1(xstrview text, int64_t width, xstrview pad, int64_t size, int64_t padSize);
XL6_TEXT_API xstrview xl6TextPadRightV1(xstrview text, int64_t width, xstrview pad, int64_t size, int64_t padSize);
XL6_TEXT_API xstrview xl6TextPadCenterV1(xstrview text, int64_t width, xstrview pad, int64_t size, int64_t padSize);
XL6_TEXT_API int64_t xl6TextLengthV1(xstrview text, int64_t size);
XL6_TEXT_API int64_t xl6TextByteLengthV1(xstrview text, int64_t size);
/* Bounded uint8 uses the language's i64 execution ABI, not foreign uint8_t. */
XL6_TEXT_API int64_t xl6TextByteAtV1(xstrview text, int64_t index, int64_t size);
XL6_TEXT_API uint32_t xl6TextCharAtV1(xstrview text, int64_t index, int64_t size);
XL6_TEXT_API xvalue* xl6TextBytesV1(xstrview text, int64_t size);
XL6_TEXT_API xvalue* xl6TextCharsV1(xstrview text, int64_t size);
XL6_TEXT_API int64_t xl6TextCompareV1(xstrview left, xstrview right, bool ignoreCase, int64_t leftSize, int64_t rightSize);
XL6_TEXT_API bool xl6TextEqualsV1(xstrview left, xstrview right, bool ignoreCase, int64_t leftSize, int64_t rightSize);
XL6_TEXT_API bool xl6TextContainsV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize);
XL6_TEXT_API int64_t xl6TextIndexOfV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize);
XL6_TEXT_API int64_t xl6TextLastIndexOfV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize);
XL6_TEXT_API int64_t xl6TextCountV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize);
XL6_TEXT_API bool xl6TextStartsWithV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize);
XL6_TEXT_API bool xl6TextEndsWithV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize);
XL6_TEXT_API bool xl6TextHasAnyV1(xstrview text, xstrview chars, int64_t size, int64_t charsSize);
XL6_TEXT_API bool xl6TextIsEmptyV1(xstrview text, int64_t size);
XL6_TEXT_API bool xl6TextIsBlankV1(xstrview text, int64_t size);
XL6_TEXT_API bool xl6TextIsUtf8V1(xstrview text, int64_t size);
XL6_TEXT_API bool xl6TextLikeV1(xstrview text, xstrview pattern, bool ignoreCase, int64_t size, int64_t patternSize);
XL6_TEXT_API bool xl6TextGlobV1(xstrview text, xstrview pattern, int64_t flags, int64_t size, int64_t patternSize);
XL6_TEXT_API int64_t xl6TextDistanceV1(xstrview left, xstrview right, int64_t limit, int64_t leftSize, int64_t rightSize);
XL6_TEXT_API double xl6TextSimilarityV1(xstrview left, xstrview right, int64_t leftSize, int64_t rightSize);
XL6_TEXT_API bool xl6TextApproxV1(xstrview left, xstrview right, double tolerance, int64_t leftSize, int64_t rightSize);
/* Arrays use the current xvalue carrier; every element is exactly STRING.
 * Split outputs own each item; join borrows input elements only for the call. */
XL6_TEXT_API xvalue* xl6TextSplitV1(xstrview text, xstrview separator, int64_t size, int64_t separatorSize);
XL6_TEXT_API xvalue* xl6TextLinesV1(xstrview text, int64_t size);
XL6_TEXT_API xvalue* xl6TextFieldsV1(xstrview text, int64_t size);
XL6_TEXT_API xstrview xl6TextJoinV1(const xvalue* values, xstrview separator, int64_t separatorSize, int64_t itemSize);
#endif
