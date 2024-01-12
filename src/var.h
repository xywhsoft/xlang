


typedef struct {
	int Type;
	int Length;
	union {
		int32 bVal;
		int8 iVal8;
		int16 iVal16;
		int32 iVal32;
		int64 iVal64;
		uint8 uVal8;
		uint16 uVal16;
		uint32 uVal32;
		uint64 uVal64;
		double dVal64;
		ptr sValPtr;
		ptr arrValObj;
		ptr tblValObj;
		ptr pVal;
	}
} xLangVariantStruct, *xLangVariant;


