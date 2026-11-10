#ifndef XIDE_SEMANTIC_COLOR_RESULT_H
#define XIDE_SEMANTIC_COLOR_RESULT_H
#include "analysis/text.h"
#include <stdint.h>
#include <xrt.h>

enum xide_color_role {
    XIDE_COLOR_TYPE, XIDE_COLOR_CLASS, XIDE_COLOR_INTERFACE, XIDE_COLOR_ENUM,
    XIDE_COLOR_STRUCT, XIDE_COLOR_TYPE_PARAMETER, XIDE_COLOR_PARAMETER,
    XIDE_COLOR_VARIABLE, XIDE_COLOR_PROPERTY, XIDE_COLOR_ENUM_MEMBER,
    XIDE_COLOR_FUNCTION, XIDE_COLOR_METHOD, XIDE_COLOR_ROLE_COUNT
};
enum xide_color_modifier { XIDE_COLOR_DECLARATION=1, XIDE_COLOR_READONLY=2, XIDE_COLOR_ASYNC=4 };
typedef struct xide_color_legend {
    unsigned typeCount, modifierCount, types[XIDE_COLOR_ROLE_COUNT], modifiers[3];
    int delta;
} xide_color_legend;
typedef struct xide_color_range { int start, end; unsigned role, modifiers; } xide_color_range;
typedef struct xide_color_result xide_color_result;
enum xide_color_error {
    XIDE_COLOR_OK, XIDE_COLOR_ARGUMENT, XIDE_COLOR_WIRE, XIDE_COLOR_BASE,
    XIDE_COLOR_COORDINATE, XIDE_COLOR_LIMIT, XIDE_COLOR_MEMORY,
    XIDE_COLOR_CANCELLED, XIDE_COLOR_REMOTE
};
/* Ordered, unique subset of the advertised native roles/modifiers. A provider
 * without full support, or with unknown names, is not enabled. Clears on error. */
int xideColorLegendParse(const xvalue* provider, xide_color_legend* legend);
/* Worker-only. Accepts null, full data, or unsorted delta edits against ONE
 * immutable old integer array. base must carry a resultId and the same legend.
 * Rejects overlap, surrogate-interior endpoints, zero lengths and out-of-line
 * tokens; malformed data leaves base unchanged and the caller uses lexical
 * highlighting. Coordinates include any original BOM. One forward UTF-8 scan.
 * budget=0 means 16 MiB; counts actual owned arrays/ID/object and temporary edit
 * storage, excluding input JSON/text/base and allocator overhead. */
xide_color_result* xideColorDecode(const xls_text* text, const xide_color_legend* legend,
    const xvalue* value, const xide_color_result* base, size_t budget,
    int (*cancelled)(void*), void* user, enum xide_color_error* error);
xide_color_result* xideColorResultRetain(xide_color_result* result);
void xideColorResultRelease(xide_color_result* result);
size_t xideColorResultCount(const xide_color_result* result);
const xide_color_range* xideColorResultRanges(const xide_color_result* result);
size_t xideColorResultWordCount(const xide_color_result* result);
const uint32_t* xideColorResultWords(const xide_color_result* result);
const char* xideColorResultId(const xide_color_result* result);
size_t xideColorResultBytes(const xide_color_result* result);
size_t xideColorResultPeakBytes(const xide_color_result* result);
size_t xideColorResultScannedBytes(const xide_color_result* result);
/* Exact lexical-span lookup; never changes the lexical cache. Caller checks
 * document identity/version/session/epoch before using this immutable result. */
const xide_color_range* xideColorResultFind(const xide_color_result*, int start, int end);

typedef struct xide_color_decoder xide_color_decoder;
typedef struct xide_color_completion {
    uint64_t token;
    enum xide_color_error error;
    xide_color_result* result;
} xide_color_completion;
/* Bounded XRT worker: one running job, one pending job, one completion and one
 * retirement slot. wake is notification only and outlives Destroy. Thread-safe
 * Submit/Discard/Retire/Cancel/Take. Only final Destroy joins. */
xide_color_decoder* xideColorDecoderCreate(void (*wake)(void*), void* user);
/* Success TAKES owned message/text/base, copies only the small legend. Failure
 * takes nothing. Tokens are strictly increasing, nonzero per decoder. message
 * is a complete JSON-RPC response. Decode/all scans/deep disposal stay on worker. */
int xideColorDecoderSubmit(xide_color_decoder*, uint64_t token, xvalue* message,
    xls_text* text, const xide_color_legend*, xide_color_result* base, size_t budget);
int xideColorDecoderDiscard(xide_color_decoder*, xvalue* ownedMessage);
/* Same pending slot, no decode/token change. Any nonempty combination; success
 * TAKES all inputs. Retire a captured source lease even if its reply never came. */
int xideColorDecoderDiscardInputs(xide_color_decoder*, xvalue* ownedMessage,
    xls_text* ownedText, xide_color_result* ownedBase);
int xideColorDecoderRetire(xide_color_decoder*, xide_color_result* ownedResult);
void xideColorDecoderCancel(xide_color_decoder*);
/* Takes a completion only if still current; cancelled answers retire on worker. */
int xideColorDecoderTake(xide_color_decoder*, xide_color_completion*);
void xideColorDecoderDestroy(xide_color_decoder*);
#endif
