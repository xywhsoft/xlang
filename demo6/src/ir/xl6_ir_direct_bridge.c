/* Included by xl6_ir.c after the canonical ABI predicates. Both lowering and
 * verification consume these closed value/resource descriptors. Cgen receives
 * explicit native-field projections, never symbol-dependent algorithms. */
typedef struct XL6_IrDirectBridgeContract {
    const char* name;
    XL6_IrRuntimeBridgeKind kind;
    XL6_IrCAbi result;
    size_t parameterCount;
    XL6_IrCAbi parameters[8];
    XL6_IrCAbi resultArrayItemAbi;
    XL6_IrIntegerTarget resultIntegerTarget;
    XL6_IrCAbi parameterArrayItemAbis[8];
    const char* resultOwner;
    const char* parameterOwners[8];
    int nullableResult;
    int noRaise;
    XL6_IrNativeProductAbi resultProductAbi;
    /* Exact synchronous checked closure profiles. A closure pointer alone is
     * not a callable signature; zero means this parameter is not a closure. */
    XL6_IrCAbi parameterNullaryCallableResultAbis[8];
    /* Collections and dynamic borrows are sealed per entry, not inferred from
     * the common xvalue* C representation. Zero preserves the array profile. */
    XL6_IrCollectionAbi resultCollectionAbi;
    unsigned char parameterDynamicBorrows[8];
    unsigned char pure;
    unsigned char nonblocking;
} XL6_IrDirectBridgeContract;

static const XL6_IrDirectBridgeContract xl6IrDirectBridgeContracts[] = {
#define S XL6_IR_C_ABI_MANAGED_STRING
#define I XL6_IR_C_ABI_I64
#define T XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE
#define B XL6_IR_C_ABI_BOOL
#define C XL6_IR_C_ABI_CHAR_U32
#define V XL6_IR_C_ABI_RUNTIME_XVALUE_PTR
#define Y XL6_IR_C_ABI_MANAGED_BYTES
#define F XL6_IR_C_ABI_F64
#define U XL6_IR_C_ABI_U64
#define M XL6_IR_C_ABI_TIME_I64
    {"xl6SignalSupportedV1",T,B,1,{I}},
    {"xl6SignalNameV1",T,S,1,{I}},
    {"xl6SignalIgnoreV1",T,B,1,{I}},
    {"xl6SignalRestoreV1",T,B,1,{I}},
    {"xl6SignalRestoreAllV1",T,B,0,{0}},
    {"xl6SignalRaiseV1",T,B,1,{I}},
    {"xl6SignalCountV1",T,U,1,{I}},
    {"xl6SignalReceivedV1",T,B,1,{I}},
    {"xl6SignalClearV1",T,XL6_IR_C_ABI_VOID,1,{I}},
    {"xl6SignalHealthyV1",T,B,0,{0}},
    {"xl6SignalResetV1",T,XL6_IR_C_ABI_VOID,0,{0}},
    {"xl6SignalQueueCreateV1",T,V,3,{I,B,I},.resultOwner="xl6SignalQueueDestroyV1"},
    {"xl6SignalQueueDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6SignalQueueDestroyV1"},.noRaise=1},
    {"xl6SignalQueueCheckOwnerV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6SignalQueueDestroyV1"}},
    {"xl6SignalQueueCloseV1",T,B,1,{V},.parameterOwners={"xl6SignalQueueDestroyV1"}},
    {"xl6SignalQueueActiveV1",T,B,1,{V},.parameterOwners={"xl6SignalQueueDestroyV1"}},
    {"xl6SignalQueueCodeV1",T,I,1,{V},.parameterOwners={"xl6SignalQueueDestroyV1"}},
    {"xl6SignalQueuePendingV1",T,I,1,{V},.parameterOwners={"xl6SignalQueueDestroyV1"}},
    {"xl6SignalQueueClosedV1",T,B,1,{V},.parameterOwners={"xl6SignalQueueDestroyV1"}},
    {"xl6SignalQueuePrepareV1",T,B,2,{V,I},.parameterOwners={"xl6SignalQueueDestroyV1"}},
    {"xl6SignalQueueSnapshotV1",T,V,1,{V},.resultOwner="xl6SignalEventDestroyV1",.parameterOwners={"xl6SignalQueueDestroyV1"}},
    {"xl6SignalQueueConsumeV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6SignalQueueDestroyV1"}},
    {"xl6SignalEventDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6SignalEventDestroyV1"},.noRaise=1},
    {"xl6SignalEventCodeV1",T,I,1,{V},.parameterOwners={"xl6SignalEventDestroyV1"}},
    {"xl6SignalEventSystemCodeV1",T,I,1,{V},.parameterOwners={"xl6SignalEventDestroyV1"}},
    {"xl6SignalEventCountV1",T,U,1,{V},.parameterOwners={"xl6SignalEventDestroyV1"}},
    {"xl6SignalEventTotalV1",T,U,1,{V},.parameterOwners={"xl6SignalEventDestroyV1"}},
    {"xl6SignalEventTimeV1",T,M,1,{V},.parameterOwners={"xl6SignalEventDestroyV1"}},
    {"xl6SignalEventCoalescedV1",T,B,1,{V},.parameterOwners={"xl6SignalEventDestroyV1"}},
    {"xl6SignalEventSaturatedV1",T,B,1,{V},.parameterOwners={"xl6SignalEventDestroyV1"}},
    {"xl6SignalEventNameV1",T,S,1,{V},.parameterOwners={"xl6SignalEventDestroyV1"}},
    {"xl6SignalTaskGroupCreateV1",T,V,0,{0},.resultOwner="xl6SignalTaskGroupDestroyV1"},
    {"xl6SignalTaskGroupDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6SignalTaskGroupDestroyV1"},.noRaise=1},
    {"xl6SignalTaskGroupCloseV1",T,B,1,{V},.parameterOwners={"xl6SignalTaskGroupDestroyV1"}},
    {"xl6SignalTaskGroupCancelV1",T,B,1,{V},.parameterOwners={"xl6SignalTaskGroupDestroyV1"}},
    {"xl6EnvGetV1",T,S,2,{S,I},.nullableResult=1},
    {"xl6EnvHasV1",T,B,2,{S,I}},
    {"xl6EnvSetV1",T,XL6_IR_C_ABI_VOID,4,{S,S,I,I}},
    {"xl6EnvRemoveV1",T,XL6_IR_C_ABI_VOID,2,{S,I}},
    {"xl6TimeFormatV1",T,S,4,{M,S,I,I}},
    {"xl6TimeRFC3339V1",T,S,2,{M,I}},
    {"xl6TimeHTTPDateV1",T,S,1,{M}},
    {"xl6TimeParsePatternV1",T,M,4,{S,S,I,I}},
    {"xl6TimeTryParsePatternV1",T,XL6_IR_C_ABI_PRODUCT,4,{S,S,I,I},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_BOOLTIME_V1},
    {"xl6TimeParseAnyV1",T,M,2,{S,I}},
    {"xl6TimeTryParseAnyV1",T,XL6_IR_C_ABI_PRODUCT,2,{S,I},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_BOOLTIME_V1},
    {"xl6TimeParseRFC3339V1",T,M,2,{S,I}},
    {"xl6TimeTryParseRFC3339V1",T,XL6_IR_C_ABI_PRODUCT,2,{S,I},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_BOOLTIME_V1},
    {"xl6TimeParseHTTPDateV1",T,M,2,{S,I}},
    {"xl6TimeTryParseHTTPDateV1",T,XL6_IR_C_ABI_PRODUCT,2,{S,I},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_BOOLTIME_V1},
    {"xl6DateTimeParseV1",T,V,4,{S,S,I,I},.resultOwner="xl6DateTimeDestroyV1"},
    {"xl6DateTimeFormatV1",T,S,3,{V,S,I},.parameterOwners={"xl6DateTimeDestroyV1"}},
    {"xl6TimeYearV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeMonthV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeDayV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeHourV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeMinuteV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeSecondV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeWeekdayV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeDayOfYearV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeQuarterV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeLeapV1",T,B,1,{I},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeDaysInYearV1",T,I,1,{I},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeDaysInMonthV1",T,I,2,{I,I}},
    {"xl6TimeDatePartV1",T,M,1,{M}},
    {"xl6TimeTimePartV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeSameDayV1",T,B,2,{M,M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeSameMonthV1",T,B,2,{M,M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeSameYearV1",T,B,2,{M,M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeInV1",T,B,3,{M,M,M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeOverlapV1",T,B,4,{M,M,M,M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeTimerV1",T,F,0,{0}},
    {"xl6TimeSleepV1",T,XL6_IR_C_ABI_VOID,1,{I}},
    {"xl6TimeMonthRangeV1",T,XL6_IR_C_ABI_PRODUCT,1,{M},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_TIMEX2_V1},
    {"xl6TimeYearRangeV1",T,XL6_IR_C_ABI_PRODUCT,1,{M},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_TIMEX2_V1},
    {"xl6TimeWeekRangeV1",T,XL6_IR_C_ABI_PRODUCT,2,{M,I},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_TIMEX2_V1},
    {"xl6TimeISOWeekV1",T,XL6_IR_C_ABI_PRODUCT,1,{M},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_I64X3_V1},
    {"xl6DateTimeSplitV1",T,V,2,{M,I},.resultOwner="xl6DateTimeDestroyV1"},
    {"xl6DateTimeFieldsV1",T,V,8,{I,I,I,I,I,I,I,I},.resultOwner="xl6DateTimeDestroyV1"},
    {"xl6DateTimeDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1},
    {"xl6DateTimeYearV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeMonthV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeDayV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeHourV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeMinuteV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeSecondV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeMicrosecondV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeOffsetV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeWeekdayV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeDayOfYearV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeIsDSTV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeToTimeV1",T,M,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"}},
    {"xl6DateTimeLocalV1",T,V,2,{M,B},.resultOwner="xl6DateTimeDestroyV1"},
    {"xl6TimeFromLocalV1",T,M,2,{V,I},.parameterOwners={"xl6DateTimeDestroyV1"}},
    {"xl6TimeTimezoneOffsetV1",T,I,1,{M}},
    {"xl6TimeFromDateTimeV1",T,M,8,{I,I,I,I,I,I,I,I}},
    {"xl6TimeFromDateV1",T,M,3,{I,I,I}},
    {"xl6TimeFromTimeV1",T,M,4,{I,I,I,I}},
    {"xl6TimeFromUnixV1",T,M,1,{I}},
    {"xl6TimeFromUnixMsV1",T,M,1,{I}},
    {"xl6TimeUnixV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeUnixMsV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeMicrosecondV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeNowV1",T,M,0,{0}},
    {"xl6TimeMonotonicV1",T,U,0,{0}},
    {"xl6TimeSleepUsV1",T,XL6_IR_C_ABI_VOID,1,{U}},
    {"xl6TimeSleepUntilV1",T,XL6_IR_C_ABI_VOID,1,{U}},
    {"xl6TimeNearV1",T,B,3,{M,M,U},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeAddV1",T,M,3,{M,I,I}},
    {"xl6TimeDiffV1",T,I,3,{M,M,I}},
#undef M
#define XL6_NATIVE_COLLECTION_BRIDGE(Name,...) {Name,__VA_ARGS__},
#include "src/abi/xl6_native_collection_contracts.inc"
#undef XL6_NATIVE_COLLECTION_BRIDGE
    {"xl6TextLowerV1",T,S,2,{S,I}},
    {"xl6TextUpperV1",T,S,2,{S,I}},
    {"xl6TextTrimV1",T,S,4,{S,S,I,I}},
    {"xl6TextLTrimV1",T,S,4,{S,S,I,I}},
    {"xl6TextRTrimV1",T,S,4,{S,S,I,I}},
    {"xl6TextFilterV1",T,S,4,{S,S,I,I}},
    {"xl6TextReplaceV1",T,S,6,{S,S,S,I,I,I}},
    {"xl6TextSliceV1",T,S,4,{S,I,I,I}},
    {"xl6TextLeftV1",T,S,3,{S,I,I}},
    {"xl6TextRightV1",T,S,3,{S,I,I}},
    {"xl6TextRepeatV1",T,S,3,{S,I,I}},
    {"xl6TextReverseV1",T,S,2,{S,I}},
    {"xl6TextRemoveAtV1",T,S,4,{S,I,I,I}},
    {"xl6TextInsertV1",T,S,5,{S,I,S,I,I}},
    {"xl6TextRemoveV1",T,S,5,{S,S,B,I,I}},
    {"xl6TextPadLeftV1",T,S,5,{S,I,S,I,I}},
    {"xl6TextPadRightV1",T,S,5,{S,I,S,I,I}},
    {"xl6TextPadCenterV1",T,S,5,{S,I,S,I,I}},
    {"xl6TextLengthV1",T,I,2,{S,I}},
    {"xl6TextByteLengthV1",T,I,2,{S,I}},
    {"xl6TextByteAtV1",T,I,3,{S,I,I},0,XL6_IR_INTEGER_TARGET_UNSIGNED_8},
    {"xl6TextCharAtV1",T,C,3,{S,I,I}},
    {"xl6TextBytesV1",T,Y,2,{S,I}},
    {"xl6TextCompareV1",T,I,5,{S,S,B,I,I}},
    {"xl6TextEqualsV1",T,B,5,{S,S,B,I,I}},
    {"xl6TextContainsV1",T,B,5,{S,S,B,I,I}},
    {"xl6TextIndexOfV1",T,I,5,{S,S,B,I,I}},
    {"xl6TextLastIndexOfV1",T,I,5,{S,S,B,I,I}},
    {"xl6TextCountV1",T,I,5,{S,S,B,I,I}},
    {"xl6TextStartsWithV1",T,B,5,{S,S,B,I,I}},
    {"xl6TextEndsWithV1",T,B,5,{S,S,B,I,I}},
    {"xl6TextHasAnyV1",T,B,4,{S,S,I,I}},
    {"xl6TextIsEmptyV1",T,B,2,{S,I}},
    {"xl6TextIsBlankV1",T,B,2,{S,I}},
    {"xl6TextIsUtf8V1",T,B,2,{S,I}},
    {"xl6TextLikeV1",T,B,5,{S,S,B,I,I}},
    {"xl6TextGlobV1",T,B,5,{S,S,I,I,I}},
    {"xl6TextDistanceV1",T,I,5,{S,S,I,I,I}},
    {"xl6TextSimilarityV1",T,F,4,{S,S,I,I}},
    {"xl6TextApproxV1",T,B,5,{S,S,F,I,I}},
    {"xl6TextIteratorCreateV1",T,V,5,{S,S,I,I,I},.resultOwner="xl6TextIteratorDestroyV1"},
    {"xl6TextIteratorDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6TextIteratorDestroyV1"},.noRaise=1},
    {"xl6TextIteratorNextV1",T,S,1,{V},.parameterOwners={"xl6TextIteratorDestroyV1"},.nullableResult=1},
    {"xl6TextIteratorCloseV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6TextIteratorDestroyV1"}},
    {"xl6TextIteratorClosedV1",T,B,1,{V},.parameterOwners={"xl6TextIteratorDestroyV1"}},
    {"xl6NumberIntFormatV1",T,S,3,{I,S,I}},
    {"xl6NumberUIntFormatV1",T,S,3,{U,S,I}},
    {"xl6NumberFloatFormatV1",T,S,3,{F,S,I}},
    {"xl6NumberBufferIntFormatV1",T,B,4,{V,I,S,I},.parameterOwners={"xl6_xrt_buffer_destroy"}},
    {"xl6NumberBufferUIntFormatV1",T,B,4,{V,U,S,I},.parameterOwners={"xl6_xrt_buffer_destroy"}},
    {"xl6NumberBufferFloatFormatV1",T,B,4,{V,F,S,I},.parameterOwners={"xl6_xrt_buffer_destroy"}},
    {"xl6NumberIntParseV1",T,I,4,{S,I,I,I}},
    {"xl6NumberInt64ParseV1",T,I,4,{S,I,I,I}},
    {"xl6NumberUIntParseV1",T,U,4,{S,I,I,I}},
    {"xl6NumberInt8ParseV1",T,I,4,{S,I,I,I},.resultIntegerTarget=XL6_IR_INTEGER_TARGET_SIGNED_8},
    {"xl6NumberInt16ParseV1",T,I,4,{S,I,I,I},.resultIntegerTarget=XL6_IR_INTEGER_TARGET_SIGNED_16},
    {"xl6NumberInt32ParseV1",T,I,4,{S,I,I,I},.resultIntegerTarget=XL6_IR_INTEGER_TARGET_SIGNED_32},
    {"xl6NumberUInt8ParseV1",T,I,4,{S,I,I,I},.resultIntegerTarget=XL6_IR_INTEGER_TARGET_UNSIGNED_8},
    {"xl6NumberUInt16ParseV1",T,I,4,{S,I,I,I},.resultIntegerTarget=XL6_IR_INTEGER_TARGET_UNSIGNED_16},
    {"xl6NumberUInt32ParseV1",T,I,4,{S,I,I,I},.resultIntegerTarget=XL6_IR_INTEGER_TARGET_UNSIGNED_32},
    {"xl6NumberFloatParseV1",T,F,3,{S,I,I}},
    {"xl6NumberIntStringV1",T,S,3,{I,I,I}},
    {"xl6NumberUIntStringV1",T,S,3,{U,I,I}},
    {"xl6NumberFloatStringV1",T,S,2,{F,I}},
    {"xl6NumberBufferIntStringV1",T,B,4,{V,I,I,I},.parameterOwners={"xl6_xrt_buffer_destroy"}},
    {"xl6NumberBufferUIntStringV1",T,B,4,{V,U,I,I},.parameterOwners={"xl6_xrt_buffer_destroy"}},
    {"xl6NumberBufferFloatStringV1",T,B,3,{V,F,I},.parameterOwners={"xl6_xrt_buffer_destroy"}},
    {"xl6UnicodeScalarV1",T,B,1,{I}},
    {"xl6UnicodeCharStringV1",T,S,1,{C}},
    {"xl6UnicodeInvalidTextV1",T,I,2,{S,I}},
    {"xl6UnicodeInvalidBytesV1",T,I,2,{Y,I}},
    {"xl6UnicodeByteOffsetV1",T,I,3,{S,I,I}},
    {"xl6UnicodeScalarIndexV1",T,I,3,{S,I,I}},
    {"xl6UnicodeValidatorCreateV1",T,V,0,{0},.resultOwner="xl6UnicodeValidatorDestroyV1"},
    {"xl6UnicodeValidatorDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6UnicodeValidatorDestroyV1"},.noRaise=1},
    {"xl6UnicodeValidatorFeedV1",T,I,4,{V,Y,B,I},.parameterOwners={"xl6UnicodeValidatorDestroyV1"}},
    {"xl6UnicodeValidatorResetV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6UnicodeValidatorDestroyV1"}},
    {"xl6UnicodeValidatorErrorV1",T,I,1,{V},.parameterOwners={"xl6UnicodeValidatorDestroyV1"}},
    {"xl6UnicodeValidatorTotalV1",T,I,1,{V},.parameterOwners={"xl6UnicodeValidatorDestroyV1"}},
    {"xl6UnicodeValidatorFinishedV1",T,B,1,{V},.parameterOwners={"xl6UnicodeValidatorDestroyV1"}},
    {"xl6HtmlEscapeTextV1",T,S,2,{S,I}},
    {"xl6HtmlEscapeAttributeV1",T,S,2,{S,I}},
    {"xl6EncodingEncodeV1",T,Y,5,{S,I,B,I,I}},
    {"xl6EncodingDecodeV1",T,S,5,{Y,I,I,I,I}},
    {"xl6EncodingTranscodeV1",T,Y,6,{Y,I,I,I,I,I}},
    {"xl6EncodingBomV1",T,I,2,{Y,I}},
    {"xl6EncodingBomSizeV1",T,I,2,{Y,I}},
    {"xl6EncodingBomBytesV1",T,Y,1,{I}},
    {"xl6EncodingUnitSizeV1",T,I,1,{I}},
    {"xl6EncodingNameV1",T,S,1,{I}},
    {"xl6EncodingRepairTextV1",T,S,2,{S,I}},
    {"xl6EncodingGuessTextV1",T,XL6_IR_C_ABI_PRODUCT,2,{S,I},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_I64X3_V1},
    {"xl6EncodingGuessBytesV1",T,XL6_IR_C_ABI_PRODUCT,2,{Y,I},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_I64X3_V1},
    {"xl6RegexCreateV1",T,V,5,{S,I,I,I,I},.resultOwner="xl6RegexDestroyV1"},
    {"xl6RegexDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexDestroyV1"},.noRaise=1},
    {"xl6RegexPatternV1",T,S,1,{V},.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexFlagsV1",T,I,1,{V},.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexCaptureCountV1",T,I,1,{V},.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexCaptureNameV1",T,S,2,{V,I},.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexCaptureIndexV1",T,I,3,{V,S,I},.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexEscapeV1",T,S,2,{S,I}},
    {"xl6RegexMatchCreateV1",T,V,5,{V,S,I,I,I},.resultOwner="xl6RegexMatchDestroyV1",.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexMatchDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexMatchDestroyV1"},.noRaise=1},
    {"xl6RegexTestV1",T,B,4,{V,S,B,I},.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexMatchMatchedV1",T,B,1,{V},.parameterOwners={"xl6RegexMatchDestroyV1"}},
    {"xl6RegexMatchCountV1",T,I,1,{V},.parameterOwners={"xl6RegexMatchDestroyV1"}},
    {"xl6RegexMatchTextV1",T,S,2,{V,I},.parameterOwners={"xl6RegexMatchDestroyV1"},.nullableResult=1},
    {"xl6RegexMatchGroupMatchedV1",T,B,2,{V,I},.parameterOwners={"xl6RegexMatchDestroyV1"}},
    {"xl6RegexMatchStartV1",T,I,2,{V,I},.parameterOwners={"xl6RegexMatchDestroyV1"}},
    {"xl6RegexMatchEndV1",T,I,2,{V,I},.parameterOwners={"xl6RegexMatchDestroyV1"}},
    {"xl6RegexMatchNameV1",T,S,2,{V,I},.parameterOwners={"xl6RegexMatchDestroyV1"}},
    {"xl6RegexMatchIndexV1",T,I,3,{V,S,I},.parameterOwners={"xl6RegexMatchDestroyV1"}},
    {"xl6RegexMatchInputV1",T,S,1,{V},.parameterOwners={"xl6RegexMatchDestroyV1"}},
    {"xl6RegexIteratorCreateV1",T,V,5,{V,S,I,I,I},.resultOwner="xl6RegexIteratorDestroyV1",.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexIteratorDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexIteratorDestroyV1"},.noRaise=1},
    {"xl6RegexIteratorHasNextV1",T,B,1,{V},.parameterOwners={"xl6RegexIteratorDestroyV1"}},
    {"xl6RegexIteratorSnapshotV1",T,V,1,{V},.resultOwner="xl6RegexMatchDestroyV1",.parameterOwners={"xl6RegexIteratorDestroyV1"}},
    {"xl6RegexIteratorConsumeV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexIteratorDestroyV1"}},
    {"xl6RegexIteratorCloseV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexIteratorDestroyV1"}},
    {"xl6RegexIteratorClosedV1",T,B,1,{V},.parameterOwners={"xl6RegexIteratorDestroyV1"}},
    {"xl6RegexReplaceV1",T,S,6,{V,S,S,I,I,I},.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexReplaceBufferV1",T,I,7,{V,V,S,S,I,I,I},.parameterOwners={"xl6RegexDestroyV1","xl6_xrt_buffer_destroy"}},
    {"xl6RegexSplitCreateV1",T,V,5,{V,S,I,I,I},.resultOwner="xl6RegexSplitDestroyV1",.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexSplitDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexSplitDestroyV1"},.noRaise=1},
    {"xl6RegexSplitHasNextV1",T,B,1,{V},.parameterOwners={"xl6RegexSplitDestroyV1"}},
    {"xl6RegexSplitSnapshotV1",T,V,1,{V},.resultOwner="xl6RegexSplitPartDestroyV1",.parameterOwners={"xl6RegexSplitDestroyV1"}},
    {"xl6RegexSplitConsumeV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexSplitDestroyV1"}},
    {"xl6RegexSplitCloseV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexSplitDestroyV1"}},
    {"xl6RegexSplitClosedV1",T,B,1,{V},.parameterOwners={"xl6RegexSplitDestroyV1"}},
    {"xl6RegexSplitPartDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexSplitPartDestroyV1"},.noRaise=1},
    {"xl6RegexSplitPartTextV1",T,S,1,{V},.parameterOwners={"xl6RegexSplitPartDestroyV1"},.nullableResult=1},
    {"xl6RegexSplitPartMatchedV1",T,B,1,{V},.parameterOwners={"xl6RegexSplitPartDestroyV1"}},
    {"xl6RegexSplitPartCaptureV1",T,I,1,{V},.parameterOwners={"xl6RegexSplitPartDestroyV1"}},
    {"xl6RegexSplitPartStartV1",T,I,1,{V},.parameterOwners={"xl6RegexSplitPartDestroyV1"}},
    {"xl6RegexSplitPartEndV1",T,I,1,{V},.parameterOwners={"xl6RegexSplitPartDestroyV1"}},
    {"xl6RegexSetDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexSetDestroyV1"},.noRaise=1},
    {"xl6RegexSetCountV1",T,I,1,{V},.parameterOwners={"xl6RegexSetDestroyV1"}},
    {"xl6RegexSetPatternV1",T,S,2,{V,I},.parameterOwners={"xl6RegexSetDestroyV1"}},
    {"xl6RegexSetFirstV1",T,I,4,{V,S,I,I},.parameterOwners={"xl6RegexSetDestroyV1"}},
    {"xl6RegexSetTestV1",T,B,4,{V,S,I,I},.parameterOwners={"xl6RegexSetDestroyV1"}},
    {"xl6RegexReplacerCreateV1",T,V,4,{V,S,I,I},.resultOwner="xl6RegexReplacerDestroyV1",.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexReplacerDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexReplacerDestroyV1"},.noRaise=1},
    {"xl6RegexReplacerSnapshotV1",T,V,1,{V},.resultOwner="xl6RegexMatchDestroyV1",.parameterOwners={"xl6RegexReplacerDestroyV1"}},
    {"xl6RegexReplacerRunV1",T,S,2,{V,XL6_IR_C_ABI_CLOSURE_PTR},.parameterOwners={"xl6RegexReplacerDestroyV1"},.parameterNullaryCallableResultAbis={0,S}},
    {"xl6RegexReplacerRunBufferV1",T,I,3,{V,V,XL6_IR_C_ABI_CLOSURE_PTR},.parameterOwners={"xl6RegexReplacerDestroyV1","xl6_xrt_buffer_destroy"},.parameterNullaryCallableResultAbis={0,0,S}},
    {"xl6RegexErrorOffsetV1",T,I,1,{XL6_IR_C_ABI_RUNTIME_ERROR_PTR},.noRaise=1},
    {"xl6RegexErrorIndexV1",T,I,1,{XL6_IR_C_ABI_RUNTIME_ERROR_PTR},.noRaise=1},
    {"xl6PatternOptionsCreateV1",T,V,7,{S,I,I,I,I,I,I},.resultOwner="xl6PatternOptionsDestroyV1"},
    {"xl6PatternOptionsDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6PatternOptionsDestroyV1"},.noRaise=1},
    {"xl6PatternOptionsSeparatorsV1",T,S,1,{V},.parameterOwners={"xl6PatternOptionsDestroyV1"}},
    {"xl6PatternOptionsBytesV1",T,I,1,{V},.parameterOwners={"xl6PatternOptionsDestroyV1"}},
    {"xl6PatternOptionsPatternsV1",T,I,1,{V},.parameterOwners={"xl6PatternOptionsDestroyV1"}},
    {"xl6PatternOptionsCapturesV1",T,I,1,{V},.parameterOwners={"xl6PatternOptionsDestroyV1"}},
    {"xl6PatternOptionsStatesV1",T,I,1,{V},.parameterOwners={"xl6PatternOptionsDestroyV1"}},
    {"xl6PatternOptionsCompiledV1",T,I,1,{V},.parameterOwners={"xl6PatternOptionsDestroyV1"}},
    {"xl6PatternCreateV1",T,V,2,{S,I},.resultOwner="xl6PatternDestroyV1"},
    {"xl6PatternCreateOptionsV1",T,V,3,{V,S,I},.resultOwner="xl6PatternDestroyV1",.parameterOwners={"xl6PatternOptionsDestroyV1"}},
    {"xl6PatternDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6PatternDestroyV1"},.noRaise=1},
    {"xl6PatternCountV1",T,I,1,{V},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternCompiledBytesV1",T,I,1,{V},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternSeparatorsV1",T,S,1,{V},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternSourceV1",T,S,2,{V,I},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternIdV1",T,U,2,{V,I},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternTagKeyV1",T,I,2,{V,I},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternCaptureCountV1",T,I,2,{V,I},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternCaptureNameV1",T,S,3,{V,I,I},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternCaptureIndexV1",T,I,4,{V,S,I,I},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternFirstV1",T,I,3,{V,S,I},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternTestV1",T,B,3,{V,S,I},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternErrorOffsetV1",T,I,1,{XL6_IR_C_ABI_RUNTIME_ERROR_PTR},.noRaise=1},
    {"xl6PatternErrorIndexV1",T,I,1,{XL6_IR_C_ABI_RUNTIME_ERROR_PTR},.noRaise=1},
    {"xl6PatternMatchCreateV1",T,V,3,{V,S,I},.resultOwner="xl6PatternMatchDestroyV1",.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternMatchDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6PatternMatchDestroyV1"},.noRaise=1},
    {"xl6PatternMatchMatchedV1",T,B,1,{V},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchCountV1",T,I,1,{V},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchPatternIndexV1",T,I,1,{V},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchIdV1",T,U,1,{V},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchInputV1",T,S,1,{V},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchSourceV1",T,S,1,{V},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchTextV1",T,S,2,{V,I},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchStartV1",T,I,2,{V,I},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchEndV1",T,I,2,{V,I},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchNameV1",T,S,2,{V,I},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchIndexV1",T,I,3,{V,S,I},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternBuilderCreateV1",T,V,0,{0},.resultOwner="xl6PatternBuilderDestroyV1"},
    {"xl6PatternBuilderOptionsV1",T,V,1,{V},.resultOwner="xl6PatternBuilderDestroyV1",.parameterOwners={"xl6PatternOptionsDestroyV1"}},
    {"xl6PatternBuilderDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6PatternBuilderDestroyV1"},.noRaise=1},
    {"xl6PatternBuilderCountV1",T,I,1,{V},.parameterOwners={"xl6PatternBuilderDestroyV1"}},
    {"xl6PatternBuilderVersionV1",T,U,1,{V},.parameterOwners={"xl6PatternBuilderDestroyV1"}},
    {"xl6PatternBuilderIdentityV1",T,V,1,{V},.parameterOwners={"xl6PatternBuilderDestroyV1"}},
    {"xl6PatternBuilderReserveV1",T,XL6_IR_C_ABI_VOID,2,{V,I},.parameterOwners={"xl6PatternBuilderDestroyV1"}},
    {"xl6PatternBuilderContainsV1",T,B,2,{V,V},.parameterOwners={"xl6PatternBuilderDestroyV1","xl6PatternKeyDestroyV1"}},
    {"xl6PatternBuilderCompileV1",T,V,1,{V},.resultOwner="xl6PatternDestroyV1",.parameterOwners={"xl6PatternBuilderDestroyV1"}},
    {"xl6PatternEditAddV1",T,V,4,{V,S,I,I},.resultOwner="xl6PatternEditDestroyV1",.parameterOwners={"xl6PatternBuilderDestroyV1"}},
    {"xl6PatternEditSetV1",T,V,5,{V,V,S,I,I},.resultOwner="xl6PatternEditDestroyV1",.parameterOwners={"xl6PatternBuilderDestroyV1","xl6PatternKeyDestroyV1"}},
    {"xl6PatternEditRemoveV1",T,V,2,{V,V},.resultOwner="xl6PatternEditDestroyV1",.parameterOwners={"xl6PatternBuilderDestroyV1","xl6PatternKeyDestroyV1"}},
    {"xl6PatternEditClearV1",T,V,1,{V},.resultOwner="xl6PatternEditDestroyV1",.parameterOwners={"xl6PatternBuilderDestroyV1"}},
    {"xl6PatternEditDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6PatternEditDestroyV1"},.noRaise=1},
    {"xl6PatternEditCountV1",T,I,1,{V},.parameterOwners={"xl6PatternEditDestroyV1"}},
    {"xl6PatternEditCancelV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6PatternEditDestroyV1"}},
    {"xl6PatternEditApplyV1",T,XL6_IR_C_ABI_VOID,3,{V,V,V},.parameterOwners={"xl6PatternEditDestroyV1"},.parameterDynamicBorrows={0,1,1}},
    {"xl6PatternKeyCreateV1",T,V,2,{V,I},.resultOwner="xl6PatternKeyDestroyV1",.parameterOwners={"xl6PatternEditDestroyV1"}},
    {"xl6PatternKeyDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6PatternKeyDestroyV1"},.noRaise=1},
    {"xl6PatternKeyIdV1",T,U,1,{V},.parameterOwners={"xl6PatternKeyDestroyV1"}},
#undef U
#undef F
#undef Y
#undef V
#undef C
#undef B
#undef S
#undef I
#undef T
    { "xl6_xrt_write", XL6_IR_RUNTIME_BRIDGE_TEXT_WRITE, XL6_IR_C_ABI_C_BOOL,
      1, { XL6_IR_C_ABI_MANAGED_STRING } },
    { "xl6_xrt_writeln", XL6_IR_RUNTIME_BRIDGE_TEXT_WRITELN, XL6_IR_C_ABI_C_BOOL,
      1, { XL6_IR_C_ABI_MANAGED_STRING } },
    { "xl6_xrt_console_error", XL6_IR_RUNTIME_BRIDGE_TEXT_ERROR, XL6_IR_C_ABI_C_BOOL,
      1, { XL6_IR_C_ABI_MANAGED_STRING } },
    { "xl6_xrt_console_error_line", XL6_IR_RUNTIME_BRIDGE_TEXT_ERRORLN, XL6_IR_C_ABI_C_BOOL,
      1, { XL6_IR_C_ABI_MANAGED_STRING } },
    { "xl6_xrt_console_flush", XL6_IR_RUNTIME_BRIDGE_TEXT_FLUSH, XL6_IR_C_ABI_C_BOOL,
      1, { XL6_IR_C_ABI_C_INT } },
    { "xl6_xrt_dir_list", XL6_IR_RUNTIME_BRIDGE_DIR_LIST, XL6_IR_C_ABI_RUNTIME_XVALUE_PTR,
      3, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_BOOL, XL6_IR_C_ABI_C_INT } },
    { "xl6_xrt_dir_glob", XL6_IR_RUNTIME_BRIDGE_DIR_GLOB, XL6_IR_C_ABI_RUNTIME_XVALUE_PTR,
      4, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_BOOL, XL6_IR_C_ABI_C_INT } },
    { "xl6_xrt_dir_entries", XL6_IR_RUNTIME_BRIDGE_DIR_ENTRIES, XL6_IR_C_ABI_RUNTIME_XVALUE_PTR,
      5, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_BOOL, XL6_IR_C_ABI_C_BOOL, XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_INT } },
    { "xl6_xrt_dir_is_empty", XL6_IR_RUNTIME_BRIDGE_DIR_IS_EMPTY, XL6_IR_C_ABI_C_BOOL,
      1, { XL6_IR_C_ABI_MANAGED_STRING } },
    { "xl6_xrt_dir_count", XL6_IR_RUNTIME_BRIDGE_DIR_COUNT, XL6_IR_C_ABI_I64,
      3, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_BOOL, XL6_IR_C_ABI_C_INT } },
    { "xl6_xrt_dir_size", XL6_IR_RUNTIME_BRIDGE_DIR_SIZE, XL6_IR_C_ABI_I64,
      2, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_BOOL } },
    /* process.exec returns an owned raw allocation; the language wrapper
     * explicitly transfers it to the existing take-cstr bridge. */
    { "xl6_xrt_process_exec", XL6_IR_RUNTIME_BRIDGE_PROCESS_EXEC, XL6_IR_C_ABI_RAW_POINTER,
      2, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_INT } },
    { "xl6_xrt_process_success", XL6_IR_RUNTIME_BRIDGE_PROCESS_SUCCESS, XL6_IR_C_ABI_C_BOOL,
      2, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_INT } },
    { "xl6_xrt_process_exit_code", XL6_IR_RUNTIME_BRIDGE_PROCESS_EXIT_CODE, XL6_IR_C_ABI_C_INT,
      2, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_INT } },
    { "xl6_xrt_hash32_string", XL6_IR_RUNTIME_BRIDGE_HASH_STRING, XL6_IR_C_ABI_C_UINT,
      2, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash32_string_seed", XL6_IR_RUNTIME_BRIDGE_HASH_STRING, XL6_IR_C_ABI_C_UINT,
      3, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_UINT, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash64_string", XL6_IR_RUNTIME_BRIDGE_HASH_STRING, XL6_IR_C_ABI_C_ULONGLONG,
      2, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash64_string_seed", XL6_IR_RUNTIME_BRIDGE_HASH_STRING, XL6_IR_C_ABI_C_ULONGLONG,
      3, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_ULONGLONG, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash32_bytes", XL6_IR_RUNTIME_BRIDGE_HASH_BYTES, XL6_IR_C_ABI_C_UINT,
      2, { XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash32_bytes_seed", XL6_IR_RUNTIME_BRIDGE_HASH_BYTES, XL6_IR_C_ABI_C_UINT,
      3, { XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_C_UINT, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash64_bytes", XL6_IR_RUNTIME_BRIDGE_HASH_BYTES, XL6_IR_C_ABI_C_ULONGLONG,
      2, { XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash64_bytes_seed", XL6_IR_RUNTIME_BRIDGE_HASH_BYTES, XL6_IR_C_ABI_C_ULONGLONG,
      3, { XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_C_ULONGLONG, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash32_raw", XL6_IR_RUNTIME_BRIDGE_HASH_RAW, XL6_IR_C_ABI_C_UINT,
      3, { XL6_IR_C_ABI_RAW_POINTER, XL6_IR_C_ABI_C_SIZE, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash32_raw_seed", XL6_IR_RUNTIME_BRIDGE_HASH_RAW, XL6_IR_C_ABI_C_UINT,
      4, { XL6_IR_C_ABI_RAW_POINTER, XL6_IR_C_ABI_C_SIZE, XL6_IR_C_ABI_C_UINT, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash64_raw", XL6_IR_RUNTIME_BRIDGE_HASH_RAW, XL6_IR_C_ABI_C_ULONGLONG,
      3, { XL6_IR_C_ABI_RAW_POINTER, XL6_IR_C_ABI_C_SIZE, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash64_raw_seed", XL6_IR_RUNTIME_BRIDGE_HASH_RAW, XL6_IR_C_ABI_C_ULONGLONG,
      4, { XL6_IR_C_ABI_RAW_POINTER, XL6_IR_C_ABI_C_SIZE, XL6_IR_C_ABI_C_ULONGLONG, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_siphash64_string", XL6_IR_RUNTIME_BRIDGE_SIPHASH_STRING, XL6_IR_C_ABI_C_ULONGLONG,
      3, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_siphash64_bytes", XL6_IR_RUNTIME_BRIDGE_SIPHASH_BYTES, XL6_IR_C_ABI_C_ULONGLONG,
      3, { XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_siphash64_raw", XL6_IR_RUNTIME_BRIDGE_SIPHASH_RAW, XL6_IR_C_ABI_C_ULONGLONG,
      4, { XL6_IR_C_ABI_RAW_POINTER, XL6_IR_C_ABI_C_SIZE, XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_I64 } }
};

static const XL6_IrDirectBridgeContract* xl6IrDirectBridgeContract(const char* name)
{
    size_t i;
    if (name == NULL) return NULL;
    for (i = 0; i < sizeof(xl6IrDirectBridgeContracts) / sizeof(xl6IrDirectBridgeContracts[0]); ++i)
        if (strcmp(name, xl6IrDirectBridgeContracts[i].name) == 0)
            return &xl6IrDirectBridgeContracts[i];
    return NULL;
}

XL6_IrRuntimeBridgeKind xl6IrRuntimeDirectBridgeKind(const char* name)
{
    const XL6_IrDirectBridgeContract* contract = xl6IrDirectBridgeContract(name);
    return contract != NULL ? contract->kind : XL6_IR_RUNTIME_BRIDGE_NONE;
}

int xl6IrRuntimeDirectBridgeFactory(const char* name)
{ const XL6_IrDirectBridgeContract* c = xl6IrDirectBridgeContract(name); return c && c->resultOwner; }
XL6_IrErrorAbi xl6IrRuntimeDirectBridgeErrorAbi(const char* name)
{
    const XL6_IrDirectBridgeContract* c = xl6IrDirectBridgeContract(name);
    return c && c->kind == XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE && !c->noRaise
        ? XL6_IR_ERROR_ABI_MAY_RAISE : XL6_IR_ERROR_ABI_NO_RAISE;
}
const char* xl6IrRuntimeDirectBridgeParameterOwner(const char* name, size_t index)
{
    const XL6_IrDirectBridgeContract* c = xl6IrDirectBridgeContract(name);
    return c && index < c->parameterCount ? c->parameterOwners[index] : NULL;
}
int xl6IrRuntimeDirectBridgeCheckedClosureParameter(const char* name,size_t index)
{
    const XL6_IrDirectBridgeContract* c=xl6IrDirectBridgeContract(name);
    return c && index<c->parameterCount && c->parameterNullaryCallableResultAbis[index]!=0;
}
static int xl6IrDirectBridgeResourceMatches(const XL6_IrModule* module,
    XL6_IrTypeId id, XL6_IrNodeId fieldId, XL6_SymbolId destroyId, const char* owner)
{
    const XL6_IrType* type = xl6IrTypeGet(&module->types, id);
    if (!type || type->kind != XL6_IR_TYPE_CLASS ||
        type->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
        type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
        type->nullAbi != XL6_IR_NULL_ABI_XVALUE_SENTINEL || fieldId == XL6_IR_NODE_INVALID) return 0;
    if (module->stage == XL6_IR_STAGE_LOWERED) {
        const XL6_IrClassLayout* layout = xl6IrBuildClassLayoutByType(module, id);
        if (!layout || layout->fieldCount != 1) return 0;
        const XL6_IrClassField* field = &module->classFields[layout->firstField];
        const XL6_IrCallTarget* destroy = xl6IrCallTargetGet(module, field->nativeDestroySymbol);
        if (field->declaration != fieldId || !destroy || !destroy->cName ||
            strcmp(destroy->cName, owner) || (destroyId && destroyId != field->nativeDestroySymbol)) return 0;
    }
    return 1;
}

static int xl6IrDirectBridgeCollectionMatches(const XL6_IrModule* module,
    const XL6_IrType* type, XL6_IrCAbi itemAbi, XL6_IrCollectionAbi collection)
{
    const XL6_IrType* item = type && type->paramCount == 1 && type->params
        ? xl6IrTypeGet(&module->types, type->params[0]) : NULL;
    if (!type || type->kind != XL6_IR_TYPE_GENERIC_INSTANCE || type->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
        type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
        type->collectionAbi != collection || type->returnCount != 0 ||
        type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
        type->nullAbi != XL6_IR_NULL_ABI_XVALUE_SENTINEL || !item ||
        item->kind != XL6_IR_TYPE_BUILTIN || item->paramCount != 0 ||
        item->collectionAbi != XL6_IR_COLLECTION_NONE || item->returnCount != 0 ||
        item->cAbi != itemAbi) return 0;
    if (itemAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR)
        return xl6IrSerializationBridgeDynamicValueType(item);
    if (itemAbi == XL6_IR_C_ABI_MANAGED_STRING)
        return item->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
            item->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL;
    if (itemAbi == XL6_IR_C_ABI_I64 && item->integerTarget != XL6_IR_INTEGER_TARGET_SIGNED_64) return 0;
    if (itemAbi == XL6_IR_C_ABI_U64 && item->integerTarget != XL6_IR_INTEGER_TARGET_UNSIGNED_64) return 0;
    return xl6IrBufferRuntimeBridgeResultMatches(item, itemAbi);
}

int xl6IrRuntimeDirectBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target)
{
    const XL6_IrDirectBridgeContract* contract;
    const XL6_IrType* result;
    size_t i;
    if (module == NULL || target == NULL ||
        (contract = xl6IrDirectBridgeContract(target->cName)) == NULL ||
        target->kind != (contract->resultOwner ? XL6_IR_CALL_TARGET_NATIVE_FACTORY : XL6_IR_CALL_TARGET_EXTERN_C) ||
        target->runtimeBridge != contract->kind || target->errorAbi !=
            xl6IrRuntimeDirectBridgeErrorAbi(contract->name) ||
        (target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_INVALID &&
         target->errorPhysicalAbi != (target->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE
            ? XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR : XL6_IR_CALL_ERROR_PHYSICAL_NONE)) ||
        (contract->resultOwner
            ? (target->resultPhysicalAbi != XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE ||
               !target->resultPhysicalDestroySymbol ||
               !xl6IrDirectBridgeResourceMatches(module, target->returnType,
                    target->resultPhysicalFieldDeclaration, target->resultPhysicalDestroySymbol, contract->resultOwner))
            : (target->resultPhysicalAbi != XL6_IR_CALL_RESULT_PHYSICAL_DIRECT ||
               target->resultPhysicalFieldDeclaration != XL6_IR_NODE_INVALID || target->resultPhysicalDestroySymbol != 0)) ||
        target->variadicAbi != XL6_IR_CALL_VARIADIC_NONE ||
        target->parameterCount != contract->parameterCount ||
        (target->parameterCount && (target->parameterTypes == NULL || target->parameterAbis == NULL ||
         target->parameterPhysicalAbis == NULL || target->parameterConstraintAbis == NULL ||
         target->parameterPhysicalFieldDeclarations == NULL))) return 0;
    result = xl6IrTypeGet(&module->types, target->returnType);
    if (contract->kind == XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE) {
        const XL6_IrNode* declaration = xl6IrNodeGet(module, target->declaration);
        if (target->effectAbi != (contract->pure ? XL6_IR_CALL_EFFECT_PURE : XL6_IR_CALL_EFFECT_CONSERVATIVE) ||
            target->executionBoundaryAbi != (contract->nonblocking
                ? XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING : XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK) ||
            !declaration || (!contract->noRaise && (declaration->flags & XL6_AST_FLAG_FOREIGN_NO_RAISE))) return 0;
        if (contract->result == XL6_IR_C_ABI_I64 && (!result ||
            result->integerTarget != (contract->resultIntegerTarget ? contract->resultIntegerTarget
                : XL6_IR_INTEGER_TARGET_SIGNED_64))) return 0;
        if (contract->result == XL6_IR_C_ABI_U64 && (!result ||
            result->integerTarget != XL6_IR_INTEGER_TARGET_UNSIGNED_64)) return 0;
    }
    if (contract->result == XL6_IR_C_ABI_VOID) {
        if (target->returnType != XL6_IR_TYPE_INVALID) return 0;
    } else if (!contract->resultOwner) {
        if (contract->resultProductAbi) {
            if (!result || result->cAbi != XL6_IR_C_ABI_PRODUCT ||
                result->nativeProductAbi != contract->resultProductAbi ||
                xl6IrNativeProductAbiForShape(&module->types,result) != contract->resultProductAbi ||
                result->ownershipAbi != XL6_IR_OWNERSHIP_ABI_TRIVIAL || result->nullAbi != XL6_IR_NULL_ABI_NONE ||
                strcmp(result->cName,xl6IrNativeProductAbiCName(contract->resultProductAbi)) != 0) return 0;
        } else if (contract->resultArrayItemAbi) {
            if (!xl6IrDirectBridgeCollectionMatches(module, result, contract->resultArrayItemAbi,
                contract->resultCollectionAbi ? contract->resultCollectionAbi : XL6_IR_COLLECTION_ARRAY)) return 0;
        } else {
            if (result == NULL || result->kind != (contract->nullableResult ? XL6_IR_TYPE_NULLABLE : XL6_IR_TYPE_BUILTIN) ||
                result->collectionAbi != XL6_IR_COLLECTION_NONE || result->returnCount != 0 ||
                result->paramCount != (contract->nullableResult ? 1 : 0)) return 0;
            if (contract->nullableResult) {
                const XL6_IrType* base = result->params ? xl6IrTypeGet(&module->types, result->params[0]) : NULL;
                if (!base || base->kind != XL6_IR_TYPE_BUILTIN || base->cAbi != contract->result ||
                    base->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED || base->paramCount ||
                    base->nullAbi != XL6_IR_NULL_ABI_POINTER_SENTINEL) return 0;
            }
            if (contract->result == XL6_IR_C_ABI_MANAGED_STRING || contract->result == XL6_IR_C_ABI_MANAGED_BYTES) {
                if (result->cAbi != contract->result || result->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
                    result->nullAbi != (contract->result == XL6_IR_C_ABI_MANAGED_STRING
                        ? XL6_IR_NULL_ABI_POINTER_SENTINEL : XL6_IR_NULL_ABI_XVALUE_SENTINEL)) return 0;
            } else if (contract->result == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR) {
                if (!xl6IrSerializationBridgeDynamicValueType(result)) return 0;
            } else if (!xl6IrBufferRuntimeBridgeResultMatches(result, contract->result)) return 0;
        }
    }
    for (i = 0; i < contract->parameterCount; ++i) {
        const XL6_IrType* type = xl6IrTypeGet(&module->types, target->parameterTypes[i]);
        XL6_IrCAbi abi = contract->parameters[i];
        if (contract->parameterNullaryCallableResultAbis[i]) {
            const XL6_IrType* returned=type && type->returnCount==1 && type->returns
                ? xl6IrTypeGet(&module->types,type->returns[0]):NULL;
            if (!type || type->kind!=XL6_IR_TYPE_FUNCTION || type->cAbi!=XL6_IR_C_ABI_CLOSURE_PTR ||
                type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED || type->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL ||
                type->collectionAbi!=XL6_IR_COLLECTION_NONE || type->paramCount || type->params ||
                type->parameterAbis || type->parameterModes || type->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_FIXED ||
                !returned || type->callableResultType!=type->returns[0] || returned->kind!=XL6_IR_TYPE_BUILTIN ||
                returned->cAbi!=contract->parameterNullaryCallableResultAbis[i] || returned->paramCount ||
                returned->returnCount || returned->collectionAbi!=XL6_IR_COLLECTION_NONE ||
                returned->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED || returned->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL ||
                target->parameterAbis[i]!=XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i]!=XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_CLOSURE_BORROW ||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID) return 0;
            continue;
        }
        if (abi==XL6_IR_C_ABI_RUNTIME_ERROR_PTR) {
            if (!type || type->kind!=XL6_IR_TYPE_BUILTIN || type->cAbi!=abi || type->paramCount || type->returnCount ||
                type->collectionAbi!=XL6_IR_COLLECTION_NONE || type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED ||
                type->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL || target->parameterAbis[i]!=XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i]!=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID) return 0;
            continue;
        }
        if (contract->parameterOwners[i]) {
            if (target->parameterAbis[i] != XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i] != XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW ||
                target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                !xl6IrDirectBridgeResourceMatches(module, target->parameterTypes[i],
                    target->parameterPhysicalFieldDeclarations[i], 0, contract->parameterOwners[i])) return 0;
            continue;
        }
        if (contract->parameterDynamicBorrows[i]) {
            if (abi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR || !xl6IrSerializationBridgeDynamicValueType(type) ||
                target->parameterAbis[i] != XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i] != XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ||
                target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i] != XL6_IR_NODE_INVALID) return 0;
            continue;
        }
        if (contract->parameterArrayItemAbis[i]) {
            if (!xl6IrDirectBridgeCollectionMatches(module, type, contract->parameterArrayItemAbis[i], XL6_IR_COLLECTION_ARRAY) ||
                target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i] != XL6_IR_NODE_INVALID ||
                !xl6IrBufferRuntimeBridgeParameterMatches(module, target, i, abi,
                    XL6_IR_PARAMETER_ABI_BORROW, XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT)) return 0;
            continue;
        }
        if (contract->kind == XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE && abi == XL6_IR_C_ABI_I64 &&
            (!type || type->integerTarget != XL6_IR_INTEGER_TARGET_SIGNED_64)) return 0;
        if (contract->kind == XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE && abi == XL6_IR_C_ABI_U64 &&
            (!type || type->integerTarget != XL6_IR_INTEGER_TARGET_UNSIGNED_64)) return 0;
        if (type == NULL || type->kind != XL6_IR_TYPE_BUILTIN ||
            type->collectionAbi != XL6_IR_COLLECTION_NONE || type->paramCount != 0 ||
            target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
            target->parameterPhysicalFieldDeclarations[i] != XL6_IR_NODE_INVALID ||
            !xl6IrBufferRuntimeBridgeParameterMatches(module, target, i, abi,
                (abi == XL6_IR_C_ABI_MANAGED_STRING || abi == XL6_IR_C_ABI_MANAGED_BYTES)
                    ? XL6_IR_PARAMETER_ABI_BORROW : XL6_IR_PARAMETER_ABI_VALUE,
                XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT)) return 0;
    }
    return 1;
}
