#ifndef XL6_PERSISTENCE_RESTORE_H
#define XL6_PERSISTENCE_RESTORE_H
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_value_domain.h"

/* Wire restoration is NOT erased-value admission. JSON loses numeric widths
 * (and encodes char as an integer); restore must normalize to the declared
 * domain before publishing a field. No strings-to-numbers, float-to-integer
 * truncation, integer wrapping, or bool/numeric coercion is permitted.
 * Borrowed input, independent owned output, NULL on a rejected wire value or
 * allocation failure. The nullable reader finish distinguishes those cases.
 * Pure metadata from verified IR selects the rule; this code never examines
 * source type spellings, class layouts, or a generated-code registry. */
static inline xvalue* xl6PersistenceRestoreScalarV1(
    const xvalue* source, unsigned int rule, bool json)
{
    xvaluetype tag;
    int64_t signedNumber;
    uint64_t unsignedNumber;
    double number;
    uint32_t scalar;
    if (source == NULL) return NULL;
    tag = xrtValueType(source);
    if (rule == XL6_VALUE_ADMIT_ANY) return xrtValueClone(source);
    if (rule >= XL6_VALUE_ADMIT_I8 && rule <= XL6_VALUE_ADMIT_U32) {
        if (tag == XVALUE_INT) {
            if (!xrtValueGetInt(source, &signedNumber)) return NULL;
        } else if (tag == XVALUE_UINT) {
            if (!xrtValueGetUInt(source, &unsignedNumber) || unsignedNumber > INT64_MAX) return NULL;
            signedNumber = (int64_t)unsignedNumber;
        } else return NULL;
        return xl6IntegerDomainV1(signedNumber, rule) ? xrtValueInt(signedNumber) : NULL;
    }
    switch (rule) {
    case XL6_VALUE_ADMIT_U64:
        if (tag == XVALUE_UINT) {
            if (!xrtValueGetUInt(source, &unsignedNumber)) return NULL;
        } else if (tag == XVALUE_INT) {
            if (!xrtValueGetInt(source, &signedNumber) || signedNumber < 0) return NULL;
            unsignedNumber = (uint64_t)signedNumber;
        } else return NULL;
        return xrtValueUInt(unsignedNumber);
    case XL6_VALUE_ADMIT_F32:
    case XL6_VALUE_ADMIT_F64:
        if (tag == XVALUE_FLOAT) {
            if (!xrtValueGetFloat(source, &number)) return NULL;
        } else if (tag == XVALUE_INT) {
            if (!xrtValueGetInt(source, &signedNumber)) return NULL;
            number = (double)signedNumber;
        } else if (tag == XVALUE_UINT) {
            if (!xrtValueGetUInt(source, &unsignedNumber)) return NULL;
            number = (double)unsignedNumber;
        } else return NULL;
        if (rule == XL6_VALUE_ADMIT_F32) {
            /* XLON admits NaN/Inf. A finite overflow is not an infinity, and
             * the range check must precede the potentially undefined cast. */
            if (isfinite(number) && (number < -FLT_MAX || number > FLT_MAX)) return NULL;
            number = (double)(float)number;
        }
        return xrtValueFloat(number);
    case XL6_VALUE_ADMIT_CHAR:
        if (tag == XVALUE_CHAR) {
            if (!xrtValueGetChar(source, &scalar)) return NULL;
        } else if (json && tag == XVALUE_INT) {
            if (!xrtValueGetInt(source, &signedNumber) || signedNumber < 0 || signedNumber > UINT32_MAX) return NULL;
            scalar = (uint32_t)signedNumber;
        } else if (json && tag == XVALUE_UINT) {
            if (!xrtValueGetUInt(source, &unsignedNumber) || unsignedNumber > UINT32_MAX) return NULL;
            scalar = (uint32_t)unsignedNumber;
        } else return NULL;
        return xl6CharDomainV1(scalar) ? xrtValueChar(scalar) : NULL;
    case XL6_VALUE_ADMIT_BOOL:
        if (tag != XVALUE_BOOL) return NULL;
        break;
    case XL6_VALUE_ADMIT_STRING:
        if (tag != XVALUE_STRING) return NULL;
        break;
    case XL6_VALUE_ADMIT_BYTES:
        if (json || tag != XVALUE_BYTES) return NULL;
        break;
    case XL6_VALUE_ADMIT_TIME:
        if (json || tag != XVALUE_TIME) return NULL;
        break;
    default: return NULL;
    }
    return xrtValueClone(source);
}

typedef enum XL6_PersistenceRestoreCollectionV1 {
    XL6_PERSIST_RESTORE_ARRAY_V1 = 1,
    XL6_PERSIST_RESTORE_LIST_V1,
    XL6_PERSIST_RESTORE_SET_V1,
    XL6_PERSIST_RESTORE_DICT_V1
} XL6_PersistenceRestoreCollectionV1;

/* Synchronous element callback, borrowed for this invocation only. Traversal
 * and rollback live here, not in a generated loop for every collection type.
 * Array/List/Set wire inputs are arrays; Dict wire inputs are objects. Shape
 * must be checked before Count/Get, even for an empty mismatched container.
 * Kind is a closed resident contract, independent of compiler enum numbers. */
static inline xvalue* xl6PersistenceRestoreCollectionV1(const xvalue* source,
    unsigned int kind, uint64_t identity, xvalue* (*restore)(const xvalue*))
{
    xvalue* result;
    size_t count;
    if (source == NULL || restore == NULL || identity == 0 ||
        kind < XL6_PERSIST_RESTORE_ARRAY_V1 || kind > XL6_PERSIST_RESTORE_DICT_V1 ||
        xrtValueType(source) != (kind == XL6_PERSIST_RESTORE_DICT_V1 ? XVALUE_OBJECT : XVALUE_ARRAY)) return NULL;
    result = kind == XL6_PERSIST_RESTORE_ARRAY_V1 ? xrtValueArray() : kind == XL6_PERSIST_RESTORE_LIST_V1 ? xrtValueIntMap() :
        kind == XL6_PERSIST_RESTORE_SET_V1 ? xrtValueSet() : xrtValueObject();
    if (result == NULL) return NULL;
    count = xrtValueCount(source);
    if (count > INT64_MAX) goto failed;
    for (size_t index = 0; index < count; ++index) {
        xstrview key = {0};
        const xvalue* borrowed = kind == XL6_PERSIST_RESTORE_DICT_V1 ? xrtValueObjectAt(source, index, &key) :
            xrtValueArrayGet(source, index);
        xvalue* item = borrowed != NULL ? restore(borrowed) : NULL;
        if (item == NULL) goto failed;
        /* Each New operation consumes item on BOTH success and failure. */
        bool ok = kind == XL6_PERSIST_RESTORE_ARRAY_V1 ? xrtValueArrayAppendNew(result, item) :
            kind == XL6_PERSIST_RESTORE_LIST_V1 ? xrtValueIntMapSetNew(result, (int64_t)index, item) :
            kind == XL6_PERSIST_RESTORE_SET_V1 ? xrtValueSetAddNew(result, item) : xrtValueObjectSetNew(result, key, item);
        if (!ok) goto failed;
    }
    if (xrtValueTypeIdBind(result, identity)) return result;
failed:
    {
        xerror* failure = xrtTakeError();
        xrtValueRelease(result);
        xrtClearError(); xrtSetErrorTake(failure);
    }
    return NULL;
}
#endif
