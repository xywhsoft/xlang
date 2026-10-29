#include "src/runtime/xl6_nominal_admission.h"
#include <assert.h>

/* Extern var parameters are borrowed. Schemas borrow the exact IDs only for
 * the predicate call, so neither the module nor its values escape this probe. */
int64_t enum_sequence_probe(xvalue* text, xvalue* integer)
{
    const uint64_t textId = xrtValueTypeId(text);
    const uint64_t integerId = xrtValueTypeId(integer);
    assert(textId && integerId && textId != integerId && !xrtGetError());
    const XL6_NominalAdmissionV1 schema = {
        XL6_NOMINAL_ADMIT_EXACT_ID, 0, &textId, 1
    };
    xrtSetErrorKind(XERR_RANGE);
    const xerror* prior = xrtGetError();
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ValueAdmitsNominalV1(text, &schema, false));
    assert(!xl6ValueAdmitsNominalV1(integer, &schema, false));
    assert(!xl6ValueAdmitsNominalV1(xrtValueNull(), &schema, false));
    assert(xl6ValueAdmitsNominalV1(xrtValueNull(), &schema, true));
    assert(!xl6ValueAdmitsNominalV1(NULL, &schema, true));
    assert(xrtGetError() == prior && !xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    xrtClearError();
    return 1;
}
