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
/* Thread-affine owned input snapshot; mode 0 split / 1 lines / 2 fields.
 * Next owns its output, NULL means EOF unless the thread error is set.
 * Close is idempotent; EOF releases inputs. Destroy is no-raise and preserves
 * an existing error. No raw handle is exposed by the language facade. */
XL6_TEXT_API void* xl6TextIteratorCreateV1(xstrview text, xstrview separator, int64_t mode, int64_t size, int64_t separatorSize);
XL6_TEXT_API void xl6TextIteratorDestroyV1(void* handle);
XL6_TEXT_API xstrview xl6TextIteratorNextV1(void* handle);
XL6_TEXT_API void xl6TextIteratorCloseV1(void* handle);
XL6_TEXT_API bool xl6TextIteratorClosedV1(void* handle);
/* Numeric format is ASCII grammar over an exact byte view, not arbitrary
 * Unicode text. size selects a clamped prefix. Results may contain U+0000. */
XL6_TEXT_API xstrview xl6NumberIntFormatV1(int64_t value, xstrview format, int64_t size);
XL6_TEXT_API xstrview xl6NumberUIntFormatV1(uint64_t value, xstrview format, int64_t size);
XL6_TEXT_API xstrview xl6NumberFloatFormatV1(double value, xstrview format, int64_t size);
XL6_TEXT_API bool xl6NumberBufferIntFormatV1(void* buffer, int64_t value, xstrview format, int64_t size);
XL6_TEXT_API bool xl6NumberBufferUIntFormatV1(void* buffer, uint64_t value, xstrview format, int64_t size);
XL6_TEXT_API bool xl6NumberBufferFloatFormatV1(void* buffer, double value, xstrview format, int64_t size);
/* Scalar parse failures return zero plus an XRT thread error, never a partial
 * value. Small integer types use i64 execution ABI with closed logical widths.
 * Default integer flags=0; float flags=8 preserves explicit NaN/Inf support. */
XL6_TEXT_API int64_t xl6NumberIntParseV1(xstrview text, int64_t base, int64_t flags, int64_t size);
XL6_TEXT_API int64_t xl6NumberInt64ParseV1(xstrview text, int64_t base, int64_t flags, int64_t size);
XL6_TEXT_API uint64_t xl6NumberUIntParseV1(xstrview text, int64_t base, int64_t flags, int64_t size);
XL6_TEXT_API int64_t xl6NumberInt8ParseV1(xstrview text, int64_t base, int64_t flags, int64_t size);
XL6_TEXT_API int64_t xl6NumberInt16ParseV1(xstrview text, int64_t base, int64_t flags, int64_t size);
XL6_TEXT_API int64_t xl6NumberInt32ParseV1(xstrview text, int64_t base, int64_t flags, int64_t size);
XL6_TEXT_API int64_t xl6NumberUInt8ParseV1(xstrview text, int64_t base, int64_t flags, int64_t size);
XL6_TEXT_API int64_t xl6NumberUInt16ParseV1(xstrview text, int64_t base, int64_t flags, int64_t size);
XL6_TEXT_API int64_t xl6NumberUInt32ParseV1(xstrview text, int64_t base, int64_t flags, int64_t size);
XL6_TEXT_API double xl6NumberFloatParseV1(xstrview text, int64_t flags, int64_t size);
XL6_TEXT_API xstrview xl6NumberIntStringV1(int64_t value, int64_t base, int64_t flags);
XL6_TEXT_API xstrview xl6NumberUIntStringV1(uint64_t value, int64_t base, int64_t flags);
XL6_TEXT_API xstrview xl6NumberFloatStringV1(double value, int64_t flags);
XL6_TEXT_API bool xl6NumberBufferIntStringV1(void* buffer, int64_t value, int64_t base, int64_t flags);
XL6_TEXT_API bool xl6NumberBufferUIntStringV1(void* buffer, uint64_t value, int64_t base, int64_t flags);
XL6_TEXT_API bool xl6NumberBufferFloatStringV1(void* buffer, double value, int64_t flags);
XL6_TEXT_API bool xl6UnicodeScalarV1(int64_t code);
XL6_TEXT_API xstrview xl6UnicodeCharStringV1(uint32_t scalar);
XL6_TEXT_API int64_t xl6UnicodeInvalidTextV1(xstrview text, int64_t size);
XL6_TEXT_API int64_t xl6UnicodeInvalidBytesV1(const xvalue* data, int64_t size);
XL6_TEXT_API int64_t xl6UnicodeByteOffsetV1(xstrview text, int64_t index, int64_t size);
XL6_TEXT_API int64_t xl6UnicodeScalarIndexV1(xstrview text, int64_t offset, int64_t size);
/* Encoding ids 0 unknown / 1 UTF8 / 2 UTF16LE / 3 BE / 4 UTF32LE / 5 BE.
 * Policy 0 strict / 1 replacement. Decode BOM modes: 0 literal, 1 optional
 * matching strip, 2 require matching strip, 3 reject any BOM. Transcode adds
 * bit 4 to emit a target BOM. Caps are bytes, not code units; all results own
 * exact independent data and failed results expose no partial allocation. */
XL6_TEXT_API xvalue* xl6EncodingEncodeV1(xstrview text, int64_t encoding, bool bom, int64_t policy, int64_t size);
XL6_TEXT_API xstrview xl6EncodingDecodeV1(const xvalue* data, int64_t encoding, int64_t policy, int64_t bom, int64_t size);
XL6_TEXT_API xvalue* xl6EncodingTranscodeV1(const xvalue* data, int64_t source, int64_t target, int64_t policy, int64_t bom, int64_t size);
XL6_TEXT_API int64_t xl6EncodingBomV1(const xvalue* data, int64_t size);
XL6_TEXT_API int64_t xl6EncodingBomSizeV1(const xvalue* data, int64_t size);
XL6_TEXT_API xvalue* xl6EncodingBomBytesV1(int64_t encoding);
XL6_TEXT_API int64_t xl6EncodingUnitSizeV1(int64_t encoding);
XL6_TEXT_API xstrview xl6EncodingNameV1(int64_t encoding);
XL6_TEXT_API xstrview xl6EncodingRepairTextV1(xstrview text, int64_t size);
/* Validator chunks are synchronous borrows. Data-invalid is an ordinary
 * XUTF_INVALID result, not a raised error; argument/lifetime/overflow faults
 * raise. final seals the stream; reset starts a new stream on its owner thread. */
XL6_TEXT_API void* xl6UnicodeValidatorCreateV1(void);
XL6_TEXT_API void xl6UnicodeValidatorDestroyV1(void* handle);
XL6_TEXT_API int64_t xl6UnicodeValidatorFeedV1(void* handle, const xvalue* data, bool final, int64_t size);
XL6_TEXT_API void xl6UnicodeValidatorResetV1(void* handle);
XL6_TEXT_API int64_t xl6UnicodeValidatorErrorV1(void* handle);
XL6_TEXT_API int64_t xl6UnicodeValidatorTotalV1(void* handle);
XL6_TEXT_API bool xl6UnicodeValidatorFinishedV1(void* handle);
XL6_TEXT_API xstrview xl6HtmlEscapeTextV1(xstrview text, int64_t size);
XL6_TEXT_API xstrview xl6HtmlEscapeAttributeV1(xstrview text, int64_t size);
#endif
