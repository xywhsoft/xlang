/* Same native vectors as XRT tests/value/test_value_object_fields.c, with
 * an embedded-snapshot test harness. Preinclude one actual implementation. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void testRequire(bool condition, const char* message)
{ if (!condition) { fprintf(stderr, "[FAIL] %s\n", message); exit(1); } }

static unsigned Finalized, LifetimeDropped, ForeignTraces;
static xvalue* ExpectedReceiver;
static bool traceNone(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{ (void)pData; (void)pVisit; (void)pContext; return true; }
static void dropNone(ptr pData) { (void)pData; ++LifetimeDropped; }
static void finalize(xvalue* pValue, ptr pData)
{
	(void)pData;
	testRequire(pValue == ExpectedReceiver, "the actual receiver, never its observation");
	xrtownershipscope Freeze = {0};
	testRequire(xrtOwnershipFreezeTryBegin(&Freeze), "finalizer outside freeze and mutation");
	testRequire(xrtOwnershipScopeEnd(&Freeze), "end finalizer probe");
	++Finalized;
}
static bool finalizeChecked(xvalue* pValue, ptr pData) { finalize(pValue, pData); return true; }
static const xvalueobjectownershipv1 Policy = {
	sizeof(Policy), finalize, traceNone, dropNone, traceNone, dropNone, finalizeChecked
};
typedef struct Retirement {
	xvalue** Values;
	unsigned Drops;
	bool ReleaseRoots;
} Retirement;
static bool retirementTrace(const xvalue* pValue, xrtownershipvisitor pVisit, ptr pContext)
{ (void)pValue; (void)pVisit; (void)pContext; return true; }
static void retirementDrop(ptr pHandle, ptr pUser)
{
	(void)pUser; Retirement* pState = pHandle;
	int64 iValue = 0;
	for (unsigned i = 0; i < 2; ++i) if (pState->Values[i])
		testRequire(xrtValueGetInt(xrtValueObjectGet(pState->Values[i], XRT_STR_LITERAL("n")), &iValue) && iValue == 42,
			"retirement sees the ENTIRE committed batch");
	xrtownershipscope Freeze = {0};
	testRequire(xrtOwnershipFreezeTryBegin(&Freeze) && xrtOwnershipScopeEnd(&Freeze), "retirement outside all fences");
	if (pState->ReleaseRoots) for (unsigned i = 0; i < 2; ++i) {
		xrtValueRelease(pState->Values[i]); pState->Values[i] = NULL;
	}
	++pState->Drops; xrtClearError(); xrtSetErrorKind(XERR_PROTOCOL);
}
static const xvaluehandleops RetirementOps = {NULL, retirementDrop, NULL, NULL};
static xvalue* receiver(bool);
static int64 number(const xvalue*);
static void stage(xvalueobjectfieldsv1*, int64);
static bool admit(xrtownershipref Reference, ptr pContext)
{
	(void)pContext;
	return xrtValueOwnershipAdapterV1(Reference) != NULL ||
		xrtValueObjectOwnershipAdapterV1(Reference, &Policy) != NULL ||
		xrtValueHandleOwnershipAdapterV1(Reference, &RetirementOps, retirementTrace) != NULL;
}
static void boundaries(void)
{
	testRequire(xrtValueObjectFieldsCommitV1(NULL, 0, NULL, 0, admit, NULL) == XVALUE_OBJECT_FIELDS_COMMITTED_V1,
		"empty batch has no receiver effect");
	testRequire(xrtValueObjectFieldsCommitV1(NULL, 1, NULL, 0, admit, NULL) == XVALUE_OBJECT_FIELDS_ERROR_V1 &&
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "malformed table"); xrtClearError();
	xvalue* pValue = receiver(false);
	xvalueobjectfieldsv1* Fields[2] = {xrtValueObjectFieldsPrepareV1(pValue), xrtValueObjectFieldsPrepareV1(pValue)};
	testRequire(Fields[0] && Fields[1], "two independently owned records");
	xrtownershipref Slot = xrtValueOwnership(pValue);
	testRequire(xrtValueObjectFieldsCommitV1(Fields, 2, &Slot, 1, admit, NULL) == XVALUE_OBJECT_FIELDS_ERROR_V1 && number(pValue) == 7,
		"duplicate actual receiver rejected before any write"); xrtClearError();
	xrtValueObjectFieldsFreeV1(Fields[1]);
	xvalue* pItem = xrtValueInt(13);
	testRequire(!xrtValueObjectFieldsSetTakeV1(Fields[0], (xstrview){NULL, 1}, &pItem) && pItem,
		"invalid prepared key keeps the exact input owner"); xrtClearError();
	testRequire(xrtValueObjectFieldsSetTakeV1(Fields[0], (xstrview){"a\0b", 3}, &pItem) && !pItem, "exact NUL key");
	stage(Fields[0], 42);
	xrtownershipscope Mutation = {0}; testRequire(xrtOwnershipMutationBegin(&Mutation), "live mutation");
	xrtSetErrorKind(XERR_TIMEOUT); const xerror* pPrimary = xrtGetError();
	testRequire(xrtValueObjectFieldsCommitV1(Fields, 1, &Slot, 1, admit, NULL) == XVALUE_OBJECT_FIELDS_BUSY_V1 &&
		xrtGetError() == pPrimary && number(pValue) == 7, "nonblocking busy preserves first diagnostic and defaults");
	testRequire(xrtOwnershipScopeEnd(&Mutation), "end busy mutation"); xrtClearError();
	testRequire(xrtValueObjectFieldsCommitV1(Fields, 1, &Slot, 1, admit, NULL) == XVALUE_OBJECT_FIELDS_COMMITTED_V1,
		"retry after busy commits once");
	int64 iValue = 0; xstrview Key = {0};
	testRequire(xrtValueGetInt(xrtValueObjectGet(pValue, (xstrview){"a\0b", 3}), &iValue) && iValue == 13 &&
		!xrtValueObjectHas(pValue, XRT_STR_LITERAL("a")) && xrtValueObjectAt(pValue, 1, &Key) && Key.Size == 3 &&
		!memcmp(Key.Data, "a\0b", 3), "NUL key and insertion order survive publication");
	testRequire(xrtValueObjectFieldsCommitV1(Fields, 1, &Slot, 1, admit, NULL) == XVALUE_OBJECT_FIELDS_ERROR_V1 && number(pValue) == 42,
		"double commit rejected"); xrtClearError();
	pItem = xrtValueInt(88);
	testRequire(pItem && !xrtValueObjectFieldsSetTakeV1(Fields[0], XRT_STR_LITERAL("n"), &pItem) && pItem,
		"committed record cannot accept another owner"); xrtClearError(); xrtValueRelease(pItem);
	xrtValueObjectFieldsFreeV1(Fields[0]); xrtValueRelease(pValue);
	for (unsigned i = 0; i < 2; ++i) {
		pValue = receiver(i != 0); ExpectedReceiver = pValue;
		xvaluecursor* pCursor = xrtValueCursorCreate(pValue);
		Fields[0] = xrtValueObjectFieldsPrepareV1(pValue); testRequire(pCursor && Fields[0], "outside native cursor");
		stage(Fields[0], 42); Slot = xrtValueOwnership(pValue);
		testRequire(xrtValueObjectFieldsCommitV1(Fields, 1, &Slot, 1, admit, NULL) ==
			(i ? XVALUE_OBJECT_FIELDS_SHARED_V1 : XVALUE_OBJECT_FIELDS_COMMITTED_V1), "native cursor protects shared identity backing");
		xvalue* pOld = NULL; xvaluekey ItemKey = {0};
		testRequire(xrtValueCursorAdvance(pCursor, &ItemKey, &pOld) == XVALUE_ITER_ITEM &&
			xrtValueGetInt(pOld, &iValue) && iValue == 7, "native cursor always observes its intact old snapshot");
		xrtValueCursorRelease(pCursor);
		if (i) testRequire(xrtValueObjectFieldsCommitV1(Fields, 1, &Slot, 1, admit, NULL) == XVALUE_OBJECT_FIELDS_COMMITTED_V1,
			"resource commit succeeds after the REAL cursor owner retires");
		xrtValueObjectFieldsFreeV1(Fields[0]); xrtValueRelease(pValue);
	}
	pValue = receiver(true); ExpectedReceiver = pValue; unsigned iFinalized = Finalized;
	Fields[0] = xrtValueObjectFieldsPrepareV1(pValue); testRequire(Fields[0] != NULL, "record protects actual receiver");
	stage(Fields[0], 42); xrtValueRelease(pValue);
	testRequire(xrtValueObjectFieldsCommitV1(Fields, 1, NULL, 0, admit, NULL) == XVALUE_OBJECT_FIELDS_COMMITTED_V1,
		"record alone owns the receiver, no invented root slot");
	xrtValueObjectFieldsFreeV1(Fields[0]);
	testRequire(Finalized == iFinalized + 1, "last record release finalizes ACTUAL receiver, not COW observation");
}
static void retirement(void)
{
	for (unsigned iMode = 0; iMode < 2; ++iMode) {
		xvalue* Values[2] = {receiver(false), receiver(false)};
		Retirement State = {Values, 0, iMode != 0};
		for (unsigned i = 0; i < 2; ++i) {
			ptr pHandle = &State; xvalue* pItem = xrtValueHandleTake(&pHandle, &RetirementOps, NULL);
			testRequire(pItem && !pHandle && xrtValueHandleOwnershipBindPhased(pItem, retirementTrace) &&
				xrtValueObjectSetNew(Values[i], XRT_STR_LITERAL("n"), pItem), "phased old field with borrowed retirement probe");
		}
		xvalueobjectfieldsv1* Fields[2] = {xrtValueObjectFieldsPrepareV1(Values[0]), xrtValueObjectFieldsPrepareV1(Values[1])};
		testRequire(Fields[0] && Fields[1], "retirement batch prepared"); stage(Fields[0], 42); stage(Fields[1], 42);
		xrtownershipref Slots[2] = {xrtValueOwnership(Values[0]), xrtValueOwnership(Values[1])};
		testRequire(xrtValueObjectFieldsCommitV1(Fields, 2, Slots, 2, admit, NULL) == XVALUE_OBJECT_FIELDS_COMMITTED_V1 && !State.Drops,
			"no producer callback during publication");
		if (iMode) xrtSetErrorKind(XERR_TIMEOUT);
		xrtValueObjectFieldsFreeV1(Fields[0]); xrtValueObjectFieldsFreeV1(Fields[1]);
		testRequire(State.Drops == 2 && xrtErrorKind(xrtGetError()) == (iMode ? XERR_TIMEOUT : XERR_PROTOCOL),
			"retirement retains the FIRST operation or cleanup error while all owners retire");
		xrtClearError(); xrtValueRelease(Values[0]); xrtValueRelease(Values[1]);
	}
}
static xvalue* receiver(bool bResource)
{
	xvalue* pValue = xrtValueObject();
	testRequire(pValue != NULL && xrtValueObjectSetNew(pValue, XRT_STR_LITERAL("n"), xrtValueInt(7)), "default object");
	if (bResource) {
		testRequire(xrtValueObjectLifetimeBindOwned(pValue, NULL, traceNone, dropNone) &&
			xrtValueObjectFinalizerPrepareOwned(pValue, finalize, NULL, traceNone, dropNone) &&
			xrtValueObjectOwnershipBindV1(pValue, &Policy) && xrtValueObjectConstructionCommit(pValue), "one committed resource duty");
	}
	return pValue;
}
static int64 number(const xvalue* pValue)
{
	int64 iValue = 0;
	testRequire(xrtValueGetInt(xrtValueObjectGet(pValue, XRT_STR_LITERAL("n")), &iValue), "read preserved value");
	return iValue;
}
static void stage(xvalueobjectfieldsv1* pFields, int64 iValue)
{
	xvalue* pItem = xrtValueInt(iValue);
	testRequire(pItem && xrtValueObjectFieldsSetTakeV1(pFields, XRT_STR_LITERAL("n"), &pItem) && !pItem, "stage one owner");
}
static void vectors(void)
{
	for (unsigned i = 0; i < 2; ++i) {
		xvalue* pValue = receiver(i != 0);
		ExpectedReceiver = pValue;
		unsigned iFinalized = Finalized;
		xvalue* pWeak = xrtValueWeakRef(pValue);
		xvalueobjectfieldsv1* pFields = xrtValueObjectFieldsPrepareV1(pValue);
		testRequire(pFields && pWeak, "prepared transaction and actual weak handle");
		stage(pFields, 42);
		testRequire(number(pValue) == 7, "preparation does not mutate defaults");
		xrtownershipref Slot = xrtValueOwnership(pValue);
		testRequire(xrtValueObjectFieldsCommitV1(&pFields, 1, &Slot, 1, admit, NULL) == XVALUE_OBJECT_FIELDS_COMMITTED_V1,
			"private receiver commits atomically");
		testRequire(number(pValue) == 42 && Finalized == iFinalized, "committed fields, no premature destructor");
		xvalue* pPromoted = xrtValueWeakRefLock(pWeak);
		testRequire(pPromoted == pValue && number(pPromoted) == 42, "weak promotion sees complete new state");
		xrtValueRelease(pPromoted);
		xrtValueObjectFieldsFreeV1(pFields);
		testRequire(Finalized == iFinalized, "free retires old data, not live receiver duty");
		xrtValueRelease(pValue);
		testRequire(Finalized == iFinalized + i && xrtValueWeakRefExpired(pWeak), "one original duty and expired weak handle");
		xrtValueRelease(pWeak);
	}
	for (unsigned i = 0; i < 2; ++i) {
		xvalue* pValue = receiver(i != 0);
		ExpectedReceiver = pValue;
		xvalue* pAlias = xrtValueClone(pValue);
		xvalueobjectfieldsv1* pFields = xrtValueObjectFieldsPrepareV1(pValue);
		testRequire(pAlias && pFields, "actual COW alias"); stage(pFields, 99);
		xrtownershipref Slot = xrtValueOwnership(pValue);
		testRequire(xrtValueObjectFieldsCommitV1(&pFields, 1, &Slot, 1, admit, NULL) ==
			(i ? XVALUE_OBJECT_FIELDS_SHARED_V1 : XVALUE_OBJECT_FIELDS_COMMITTED_V1), "COW detaches; shared resource refuses");
		testRequire(number(pAlias) == 7 && number(pValue) == (i ? 7 : 99), "outside alias not modified");
		xrtValueObjectFieldsFreeV1(pFields); xrtValueRelease(pAlias); xrtValueRelease(pValue);
	}
	{
		xvalue* pValue = receiver(false), *pOutside = xrtValueRetain(pValue);
		xvalueobjectfieldsv1* pFields = xrtValueObjectFieldsPrepareV1(pValue);
		testRequire(pOutside && pFields, "external reference"); stage(pFields, 20);
		xrtownershipref Slots[3] = {xrtValueOwnership(pValue), xrtValueOwnership(pOutside), xrtValueOwnership(pValue)};
		testRequire(xrtValueObjectFieldsCommitV1(&pFields, 1, Slots, 1, admit, NULL) == XVALUE_OBJECT_FIELDS_SHARED_V1 && number(pValue) == 7,
			"unaccounted external strong owner is shared");
		testRequire(xrtValueObjectFieldsCommitV1(&pFields, 1, Slots, 3, admit, NULL) == XVALUE_OBJECT_FIELDS_ERROR_V1 && number(pValue) == 7,
			"overcounted internal slots refuse without mutation"); xrtClearError();
		testRequire(xrtValueObjectFieldsCommitV1(&pFields, 1, Slots, 2, admit, NULL) == XVALUE_OBJECT_FIELDS_COMMITTED_V1,
			"two REAL transaction owners count with multiplicity");
		xrtValueObjectFieldsFreeV1(pFields); xrtValueRelease(pOutside); xrtValueRelease(pValue);
	}
	{
		xvalue* pValue = receiver(false);
		xvalueobjectfieldsv1* pFields = xrtValueObjectFieldsPrepareV1(pValue); testRequire(pFields != NULL, "stale fixture");
		stage(pFields, 42);
		testRequire(xrtValueObjectSetNew(pValue, XRT_STR_LITERAL("n"), xrtValueInt(8)), "real intervening write");
		xrtownershipref Slot = xrtValueOwnership(pValue);
		testRequire(xrtValueObjectFieldsCommitV1(&pFields, 1, &Slot, 1, admit, NULL) == XVALUE_OBJECT_FIELDS_ERROR_V1 && number(pValue) == 8,
			"changed observation refuses, no lost update"); xrtClearError();
		xrtValueObjectFieldsFreeV1(pFields); xrtValueRelease(pValue);
	}
}
static void cycles(void)
{
	xvalue* pA = receiver(false), *pB = receiver(false);
	xvalueobjectfieldsv1* Fields[2] = {xrtValueObjectFieldsPrepareV1(pA), xrtValueObjectFieldsPrepareV1(pB)};
	testRequire(Fields[0] && Fields[1], "two independent prepared stores");
	xvalue* pItem = xrtValueRetain(pB);
	testRequire(pItem && xrtValueObjectFieldsSetTakeV1(Fields[0], XRT_STR_LITERAL("b"), &pItem), "first acyclic edit");
	pItem = xrtValueRetain(pA);
	testRequire(pItem && xrtValueObjectFieldsSetTakeV1(Fields[1], XRT_STR_LITERAL("a"), &pItem), "second individually acyclic edit");
	xrtownershipref Slots[2] = {xrtValueOwnership(pA), xrtValueOwnership(pB)};
	testRequire(xrtValueObjectFieldsCommitV1(Fields, 2, Slots, 2, admit, NULL) == XVALUE_OBJECT_FIELDS_ERROR_V1 &&
		!xrtValueObjectHas(pA, XRT_STR_LITERAL("b")) && !xrtValueObjectHas(pB, XRT_STR_LITERAL("a")), "combined cycle refuses ALL writes");
	xrtClearError(); xrtValueObjectFieldsFreeV1(Fields[1]); xrtValueObjectFieldsFreeV1(Fields[0]);
	xrtValueRelease(pB); xrtValueRelease(pA);
	for (unsigned iMode = 0; iMode < 2; ++iMode) {
		pA = receiver(true); pB = receiver(true);
		Fields[0] = xrtValueObjectFieldsPrepareV1(pA); Fields[1] = xrtValueObjectFieldsPrepareV1(pB);
		testRequire(Fields[0] && Fields[1], "physical identity cycle fixtures");
		pItem = xrtValueClone(iMode ? pB : pA);
		testRequire(pItem && xrtValueObjectFieldsSetTakeV1(Fields[0], XRT_STR_LITERAL("alias"), &pItem) && !pItem,
			"consumed different shell for the SAME reference-identity backing");
		if (iMode) {
			pItem = xrtValueClone(pA);
			testRequire(pItem && xrtValueObjectFieldsSetTakeV1(Fields[1], XRT_STR_LITERAL("alias"), &pItem) && !pItem,
				"two individually acyclic identity-alias edits");
		}
		Slots[0] = xrtValueOwnership(pA); Slots[1] = xrtValueOwnership(pB);
		testRequire(xrtValueObjectFieldsCommitV1(Fields, 2, Slots, 2, admit, NULL) == XVALUE_OBJECT_FIELDS_ERROR_V1 &&
			!xrtValueObjectHas(pA, XRT_STR_LITERAL("alias")) && !xrtValueObjectHas(pB, XRT_STR_LITERAL("alias")),
			"physical backing self or mutual cycle refuses ALL writes");
		xrtClearError(); xrtValueObjectFieldsFreeV1(Fields[1]); xrtValueObjectFieldsFreeV1(Fields[0]);
		ExpectedReceiver = pB; xrtValueRelease(pB); ExpectedReceiver = pA; xrtValueRelease(pA);
	}
}
static bool foreignTrace(const xvalue* pValue, xrtownershipvisitor pVisit, ptr pContext)
{ (void)pValue; (void)pVisit; (void)pContext; ++ForeignTraces; return true; }
static void foreignDrop(ptr pData, ptr pUser) { (void)pData; (void)pUser; }
static void unknown(void)
{
	static const xvaluehandleops Ops = {NULL, foreignDrop, NULL, NULL};
	xvalue* pValue = receiver(false); ptr pHandle = &ForeignTraces;
	xvalue* pUnknown = xrtValueHandleTake(&pHandle, &Ops, NULL);
	testRequire(pUnknown && xrtValueHandleOwnershipBind(pUnknown, foreignTrace) &&
		xrtValueObjectSetNew(pValue, XRT_STR_LITERAL("unknown"), pUnknown), "unadmitted transitive handle");
	xvalueobjectfieldsv1* pFields = xrtValueObjectFieldsPrepareV1(pValue); testRequire(pFields != NULL, "unknown preparation");
	stage(pFields, 42); xrtownershipref Slot = xrtValueOwnership(pValue);
	testRequire(xrtValueObjectFieldsCommitV1(&pFields, 1, &Slot, 1, admit, NULL) == XVALUE_OBJECT_FIELDS_ERROR_V1 &&
		ForeignTraces == 0 && number(pValue) == 7, "reject BEFORE foreign Trace, never infer clone on unknown");
	xrtClearError(); xrtValueObjectFieldsFreeV1(pFields); xrtValueRelease(pValue);
}
#if defined(XRT_FEATURE_MEMORY_DEBUG)
static unsigned faults(void)
{
	unsigned iPrefixes = 0;
	for (unsigned iMode = 0; iMode < 4; ++iMode) {
		bool bClosed = false;
		for (unsigned iPoint = 0; iPoint < 2048; ++iPoint) {
			unsigned iFinalized = Finalized;
			xvalue* Values[2] = {receiver(iMode == 3), receiver(iMode == 3)};
			xvalueobjectfieldsv1* Fields[2] = {NULL, NULL};
			xrtownershipref Slots[2] = {xrtValueOwnership(Values[0]), xrtValueOwnership(Values[1])};
			if (iMode == 1) {
				for (unsigned i = 0; i < 2; ++i) { Fields[i] = xrtValueObjectFieldsPrepareV1(Values[i]); testRequire(Fields[i] != NULL, "commit-only setup"); stage(Fields[i], 42); }
			}
			testRequire(xrtMemDebugFailAfter(iPoint), "arm consecutive real allocation failure");
			bool bReady = true;
			char KeyData[512]; memset(KeyData, 'k', sizeof(KeyData)); KeyData[255] = '\0';
			if (iMode != 1) for (unsigned i = 0; bReady && i < 2; ++i) {
				Fields[i] = xrtValueObjectFieldsPrepareV1(Values[i]);
				xvalue* pItem = Fields[i] ? xrtValueInt(42) : NULL;
				bReady = pItem && xrtValueObjectFieldsSetTakeV1(Fields[i], XRT_STR_LITERAL("n"), &pItem);
				xrtValueRelease(pItem);
				if (bReady && iMode == 2) {
					pItem = xrtValueInt(55);
					bReady = pItem && xrtValueObjectFieldsSetTakeV1(Fields[i], (xstrview){KeyData, sizeof(KeyData)}, &pItem);
					xrtValueRelease(pItem);
				}
			}
			xvalueobjectfieldsresultv1 Result = bReady ? xrtValueObjectFieldsCommitV1(Fields, 2, Slots, 2, admit, NULL) : XVALUE_OBJECT_FIELDS_ERROR_V1;
			bool bHit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xrtClearError();
			testRequire(bHit ? Result == XVALUE_OBJECT_FIELDS_ERROR_V1 && number(Values[0]) == 7 && number(Values[1]) == 7 :
				Result == XVALUE_OBJECT_FIELDS_COMMITTED_V1 && number(Values[0]) == 42 && number(Values[1]) == 42, "every failure preserves BOTH default objects");
			for (unsigned i = 0; i < 2; ++i) {
				int64 iValue = 0;
				if (iMode == 2) testRequire(bHit ? !xrtValueObjectHas(Values[i], (xstrview){KeyData, sizeof(KeyData)}) :
					xrtValueGetInt(xrtValueObjectGet(Values[i], (xstrview){KeyData, sizeof(KeyData)}), &iValue) && iValue == 55,
					"new long NUL key also remains batch-atomic under continuous failure");
				ExpectedReceiver = Values[i]; xrtValueObjectFieldsFreeV1(Fields[i]); xrtValueRelease(Values[i]);
			}
			testRequire(Finalized == iFinalized + (iMode == 3 ? 2 : 0), "failed or committed resource preparation preserves TWO duties");
			++iPrefixes;
			if (!bHit) { printf("Object fields mode=%u first-no-hit=%u closed-prefix\n", iMode, iPoint); bClosed = true; break; }
		}
		testRequire(bClosed, "fault prefix must actually close");
	}
	return iPrefixes;
}
#endif
int main(void)
{
#if defined(XRT_FEATURE_MEMORY_DEBUG)
	testRequire(xrtMemDebugEnable(true), "actual allocator ledger");
	xmemdebugsnapshot Before = {0}, After = {0}; xrtMemDebugSnapshot(&Before);
#endif
	vectors(); boundaries(); retirement(); cycles(); unknown(); testRequire(!xrtGetError(), "no residual diagnostics");
#if defined(XRT_FEATURE_MEMORY_DEBUG)
	unsigned iPrefixes = faults(); xrtMemDebugSnapshot(&After);
	testRequire(Before.LiveCount == After.LiveCount && Before.LiveBytes == After.LiveBytes &&
		After.AllocCount - Before.AllocCount == After.FreeCount - Before.FreeCount &&
		Before.InvalidFreeCount == After.InvalidFreeCount && Before.DoubleFreeCount == After.DoubleFreeCount &&
		Before.UseAfterFreeCount == After.UseAfterFreeCount, "balanced actual owners, no invalid release");
	printf("Object fields: %u real allocation-failure prefixes balanced\n", iPrefixes);
#endif
	puts("Object fields: complete batch, strong/COW/weak owners, native duty, changed input, planned cycle and pre-trace refusal verified");
	return 0;
}
