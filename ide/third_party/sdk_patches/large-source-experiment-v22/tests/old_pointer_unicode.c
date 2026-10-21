#include "xui_internal.h"

#include <string.h>

typedef enum xui_grapheme_class_t {
	XUI_GRAPHEME_OTHER = 0,
	XUI_GRAPHEME_CR,
	XUI_GRAPHEME_LF,
	XUI_GRAPHEME_CONTROL,
	XUI_GRAPHEME_EXTEND,
	XUI_GRAPHEME_ZWJ,
	XUI_GRAPHEME_REGIONAL,
	XUI_GRAPHEME_PREPEND,
	XUI_GRAPHEME_SPACING_MARK,
	XUI_GRAPHEME_HANGUL_L,
	XUI_GRAPHEME_HANGUL_V,
	XUI_GRAPHEME_HANGUL_T,
	XUI_GRAPHEME_HANGUL_LV,
	XUI_GRAPHEME_HANGUL_LVT
} xui_grapheme_class_t;

typedef enum xui_word_break_property_t {
	XUI_WB_OTHER = 0,
	XUI_WB_ALETTER,
	XUI_WB_CR,
	XUI_WB_DOUBLE_QUOTE,
	XUI_WB_EXTEND,
	XUI_WB_EXTEND_NUM_LET,
	XUI_WB_FORMAT,
	XUI_WB_HEBREW_LETTER,
	XUI_WB_KATAKANA,
	XUI_WB_LF,
	XUI_WB_MID_LETTER,
	XUI_WB_MID_NUM,
	XUI_WB_MID_NUM_LET,
	XUI_WB_NEWLINE,
	XUI_WB_NUMERIC,
	XUI_WB_REGIONAL,
	XUI_WB_SINGLE_QUOTE,
	XUI_WB_WSEG_SPACE,
	XUI_WB_ZWJ
} xui_word_break_property_t;

typedef struct xui_unicode_word_range_t {
	uint32_t iFirst;
	uint32_t iLast;
	uint8_t iProperty;
} xui_unicode_word_range_t;

typedef struct xui_unicode_binary_range_t {
	uint32_t iFirst;
	uint32_t iLast;
} xui_unicode_binary_range_t;

typedef struct xui_unicode_word_unit_t {
	int iStart;
	int iEnd;
	uint32_t iCodepoint;
	xui_word_break_property_t iProperty;
	int bXidContinue;
} xui_unicode_word_unit_t;

#include "xui_unicode_word_data.inc"

static int __xuiUnicodeRange(uint32_t iCodepoint, uint32_t iFirst, uint32_t iLast)
{
	return iCodepoint >= iFirst && iCodepoint <= iLast;
}





#include "xui_unicode_grapheme_data.inc"
static xui_grapheme_class_t __xuiUnicodeClass(uint32_t iCodepoint)
{
    if ( iCodepoint <= 0x10FFFFu ) {
        return (xui_grapheme_class_t)__xuiUnicodeGraphemeClasses[
            __xuiUnicodeGraphemePages[iCodepoint >> 8]][iCodepoint & 255u];
    }
    return XUI_GRAPHEME_OTHER;
}

static int __xuiUnicodePointerRead(void* pUser, int iOffset, unsigned char* pByte)
{
	const unsigned char* pText;

	if ( pUser == NULL || pByte == NULL || iOffset < 0 ) return 0;
	pText = (const unsigned char*)pUser;
	*pByte = pText[iOffset];
	return 1;
}

static int __xuiUnicodeDecode(xui_internal_text_read_proc onRead, void* pUser,
	int iLength, int iOffset, uint32_t* pCodepoint, int* pNext)
{
	unsigned char c0;
	unsigned char c1;
	unsigned char c2;
	unsigned char c3;
	uint32_t iCodepoint;
	int iStep;

	if ( pCodepoint != NULL ) *pCodepoint = 0xFFFDu;
	if ( pNext != NULL ) *pNext = iOffset;
	if ( onRead == NULL || iOffset < 0 || iOffset >= iLength ||
	     !onRead(pUser, iOffset, &c0) ) return 0;
	iCodepoint = c0;
	iStep = 1;
	if ( c0 < 0x80u ) {
		iCodepoint = c0;
	} else if ( (c0 & 0xE0u) == 0xC0u && iOffset + 1 < iLength &&
	            onRead(pUser, iOffset + 1, &c1) && (c1 & 0xC0u) == 0x80u ) {
		iCodepoint = ((uint32_t)(c0 & 0x1Fu) << 6) | (uint32_t)(c1 & 0x3Fu);
		if ( iCodepoint >= 0x80u ) iStep = 2;
		else iCodepoint = 0xFFFDu;
	} else if ( (c0 & 0xF0u) == 0xE0u && iOffset + 2 < iLength &&
	            onRead(pUser, iOffset + 1, &c1) && onRead(pUser, iOffset + 2, &c2) &&
	            (c1 & 0xC0u) == 0x80u && (c2 & 0xC0u) == 0x80u ) {
		iCodepoint = ((uint32_t)(c0 & 0x0Fu) << 12) |
			((uint32_t)(c1 & 0x3Fu) << 6) | (uint32_t)(c2 & 0x3Fu);
		if ( iCodepoint >= 0x800u && !__xuiUnicodeRange(iCodepoint, 0xD800u, 0xDFFFu) ) iStep = 3;
		else iCodepoint = 0xFFFDu;
	} else if ( (c0 & 0xF8u) == 0xF0u && iOffset + 3 < iLength &&
	            onRead(pUser, iOffset + 1, &c1) && onRead(pUser, iOffset + 2, &c2) &&
	            onRead(pUser, iOffset + 3, &c3) &&
	            (c1 & 0xC0u) == 0x80u && (c2 & 0xC0u) == 0x80u && (c3 & 0xC0u) == 0x80u ) {
		iCodepoint = ((uint32_t)(c0 & 0x07u) << 18) |
			((uint32_t)(c1 & 0x3Fu) << 12) |
			((uint32_t)(c2 & 0x3Fu) << 6) | (uint32_t)(c3 & 0x3Fu);
		if ( iCodepoint >= 0x10000u && iCodepoint <= 0x10FFFFu ) iStep = 4;
		else iCodepoint = 0xFFFDu;
	}
	if ( pCodepoint != NULL ) *pCodepoint = iCodepoint;
	if ( pNext != NULL ) *pNext = iOffset + iStep;
	return 1;
}

static int __xuiUnicodePrevCodepoint(xui_internal_text_read_proc onRead, void* pUser,
	int iLength, int iOffset, uint32_t* pCodepoint)
{
	unsigned char c;
	int iStart;
	int iNext;

	if ( iOffset <= 0 ) return 0;
	if ( iOffset > iLength ) iOffset = iLength;
	iStart = iOffset - 1;
	while ( iStart > 0 && onRead(pUser, iStart, &c) && (c & 0xC0u) == 0x80u ) iStart--;
	if ( !__xuiUnicodeDecode(onRead, pUser, iLength, iStart, pCodepoint, &iNext) ) return iOffset - 1;
	if ( iNext > iOffset ) {
		if ( pCodepoint != NULL ) *pCodepoint = 0xFFFDu;
		return iOffset - 1;
	}
	return iStart;
}

static int __xuiUnicodeHangulJoins(xui_grapheme_class_t iLeft, xui_grapheme_class_t iRight)
{
	if ( iLeft == XUI_GRAPHEME_HANGUL_L &&
	     (iRight == XUI_GRAPHEME_HANGUL_L || iRight == XUI_GRAPHEME_HANGUL_V ||
	      iRight == XUI_GRAPHEME_HANGUL_LV || iRight == XUI_GRAPHEME_HANGUL_LVT) ) return 1;
	if ( (iLeft == XUI_GRAPHEME_HANGUL_LV || iLeft == XUI_GRAPHEME_HANGUL_V) &&
	     (iRight == XUI_GRAPHEME_HANGUL_V || iRight == XUI_GRAPHEME_HANGUL_T) ) return 1;
	if ( (iLeft == XUI_GRAPHEME_HANGUL_LVT || iLeft == XUI_GRAPHEME_HANGUL_T) &&
	     iRight == XUI_GRAPHEME_HANGUL_T ) return 1;
	return 0;
}

int xuiInternalTextGraphemeNextRead(xui_internal_text_read_proc onRead,
	void* pUser, int iLength, int iOffset)
{
	xui_grapheme_class_t iFirstClass;
	xui_grapheme_class_t iPreviousClass;
	xui_grapheme_class_t iNextClass;
	uint32_t iCodepoint;
	int iNext;
	int iAfter;
	int iRegionalCount;
	int bJoinAfterZwj;

	if ( onRead == NULL || iLength <= 0 ) return 0;
	if ( iOffset < 0 ) iOffset = 0;
	if ( iOffset >= iLength ) return iLength;
	if ( !__xuiUnicodeDecode(onRead, pUser, iLength, iOffset, &iCodepoint, &iNext) ) return iOffset;
	iFirstClass = __xuiUnicodeClass(iCodepoint);
	iPreviousClass = iFirstClass;
	iRegionalCount = (iFirstClass == XUI_GRAPHEME_REGIONAL) ? 1 : 0;
	bJoinAfterZwj = 0;
	while ( iNext < iLength ) {
		if ( !__xuiUnicodeDecode(onRead, pUser, iLength, iNext, &iCodepoint, &iAfter) ) break;
		iNextClass = __xuiUnicodeClass(iCodepoint);
		if ( iPreviousClass == XUI_GRAPHEME_CR && iNextClass == XUI_GRAPHEME_LF ) {
			iNext = iAfter;
			break;
		}
		if ( iPreviousClass == XUI_GRAPHEME_CR || iPreviousClass == XUI_GRAPHEME_LF ||
		     iPreviousClass == XUI_GRAPHEME_CONTROL || iNextClass == XUI_GRAPHEME_CR ||
		     iNextClass == XUI_GRAPHEME_LF || iNextClass == XUI_GRAPHEME_CONTROL ) break;
		if ( __xuiUnicodeHangulJoins(iPreviousClass, iNextClass) ||
		     iNextClass == XUI_GRAPHEME_EXTEND ||
		     iNextClass == XUI_GRAPHEME_SPACING_MARK ||
		     iNextClass == XUI_GRAPHEME_ZWJ ||
		     iPreviousClass == XUI_GRAPHEME_PREPEND ||
		     bJoinAfterZwj ) {
			bJoinAfterZwj = (iNextClass == XUI_GRAPHEME_ZWJ) ? 1 :
				(bJoinAfterZwj && (iNextClass == XUI_GRAPHEME_EXTEND ||
				                  iNextClass == XUI_GRAPHEME_SPACING_MARK));
			iPreviousClass = iNextClass;
			iNext = iAfter;
			continue;
		}
		if ( iPreviousClass == XUI_GRAPHEME_REGIONAL &&
		     iNextClass == XUI_GRAPHEME_REGIONAL && (iRegionalCount & 1) != 0 ) {
			iRegionalCount++;
			iPreviousClass = iNextClass;
			iNext = iAfter;
			continue;
		}
		break;
	}
	return iNext;
}

int xuiInternalTextGraphemePrevRead(xui_internal_text_read_proc onRead,
	void* pUser, int iLength, int iOffset)
{
	xui_grapheme_class_t iClass;
	xui_grapheme_class_t iPreviousClass;
	uint32_t iCodepoint;
	uint32_t iPreviousCodepoint;
	int iStart;
	int iPrevious;
	int iImmediateRegional;
	int iRegionalCount;

	if ( onRead == NULL || iLength <= 0 || iOffset <= 0 ) return 0;
	if ( iOffset > iLength ) iOffset = iLength;
	iStart = __xuiUnicodePrevCodepoint(onRead, pUser, iLength, iOffset, &iCodepoint);
	iClass = __xuiUnicodeClass(iCodepoint);
	if ( iClass == XUI_GRAPHEME_LF && iStart > 0 ) {
		iPrevious = __xuiUnicodePrevCodepoint(onRead, pUser, iLength, iStart, &iPreviousCodepoint);
		if ( __xuiUnicodeClass(iPreviousCodepoint) == XUI_GRAPHEME_CR ) return iPrevious;
	}
	if ( iClass == XUI_GRAPHEME_CR || iClass == XUI_GRAPHEME_LF ||
	     iClass == XUI_GRAPHEME_CONTROL ) return iStart;

	while ( iStart > 0 && (iClass == XUI_GRAPHEME_EXTEND ||
	       iClass == XUI_GRAPHEME_SPACING_MARK) ) {
		iStart = __xuiUnicodePrevCodepoint(onRead, pUser, iLength, iStart, &iCodepoint);
		iClass = __xuiUnicodeClass(iCodepoint);
	}

	if ( iClass == XUI_GRAPHEME_REGIONAL ) {
		iImmediateRegional = iStart;
		iRegionalCount = 1;
		iPrevious = iStart;
		while ( iPrevious > 0 ) {
			iPrevious = __xuiUnicodePrevCodepoint(onRead, pUser, iLength, iPrevious, &iPreviousCodepoint);
			if ( __xuiUnicodeClass(iPreviousCodepoint) != XUI_GRAPHEME_REGIONAL ) break;
			if ( iRegionalCount == 1 ) iImmediateRegional = iPrevious;
			iRegionalCount++;
		}
		return ((iRegionalCount & 1) == 0) ? iImmediateRegional : iStart;
	}

	while ( iStart > 0 ) {
		iPrevious = __xuiUnicodePrevCodepoint(onRead, pUser, iLength, iStart, &iPreviousCodepoint);
		iPreviousClass = __xuiUnicodeClass(iPreviousCodepoint);
		if ( !__xuiUnicodeHangulJoins(iPreviousClass, iClass) ) break;
		iStart = iPrevious;
		iClass = iPreviousClass;
	}

	for ( ;; ) {
		if ( iClass == XUI_GRAPHEME_ZWJ ) {
			if ( iStart <= 0 ) break;
			iStart = __xuiUnicodePrevCodepoint(onRead, pUser, iLength, iStart, &iCodepoint);
			iClass = __xuiUnicodeClass(iCodepoint);
			while ( iStart > 0 && (iClass == XUI_GRAPHEME_EXTEND ||
			       iClass == XUI_GRAPHEME_SPACING_MARK) ) {
				iStart = __xuiUnicodePrevCodepoint(onRead, pUser, iLength, iStart, &iCodepoint);
				iClass = __xuiUnicodeClass(iCodepoint);
			}
			continue;
		}
		if ( iStart <= 0 ) break;
		iPrevious = __xuiUnicodePrevCodepoint(onRead, pUser, iLength, iStart, &iPreviousCodepoint);
		iPreviousClass = __xuiUnicodeClass(iPreviousCodepoint);
		if ( iPreviousClass != XUI_GRAPHEME_ZWJ ) break;
		iStart = iPrevious;
		iClass = iPreviousClass;
	}
	while ( iStart > 0 ) {
		iPrevious = __xuiUnicodePrevCodepoint(onRead, pUser, iLength, iStart, &iPreviousCodepoint);
		if ( __xuiUnicodeClass(iPreviousCodepoint) != XUI_GRAPHEME_PREPEND ) break;
		iStart = iPrevious;
	}
	return iStart;
}

int xuiInternalTextGraphemeClampRead(xui_internal_text_read_proc onRead,
	void* pUser, int iLength, int iOffset)
{
	unsigned char c;
	int iPrevious;
	int iNext;

	if ( onRead == NULL || iLength <= 0 || iOffset <= 0 ) return 0;
	if ( iOffset >= iLength ) return iLength;
	while ( iOffset > 0 && onRead(pUser, iOffset, &c) && (c & 0xC0u) == 0x80u ) iOffset--;
	if ( iOffset <= 0 ) return 0;
	iPrevious = xuiInternalTextGraphemePrevRead(onRead, pUser, iLength, iOffset);
	iNext = xuiInternalTextGraphemeNextRead(onRead, pUser, iLength, iPrevious);
	return (iNext > iOffset) ? iPrevious : iOffset;
}

int xuiInternalTextGraphemeNext(const char* sText, int iLength, int iOffset)
{
	if ( sText == NULL ) return 0;
	if ( iLength < 0 ) iLength = (int)strlen(sText);
	return xuiInternalTextGraphemeNextRead(__xuiUnicodePointerRead,
		(void*)sText, iLength, iOffset);
}

int xuiInternalTextGraphemePrev(const char* sText, int iLength, int iOffset)
{
	if ( sText == NULL ) return 0;
	if ( iLength < 0 ) iLength = (int)strlen(sText);
	return xuiInternalTextGraphemePrevRead(__xuiUnicodePointerRead,
		(void*)sText, iLength, iOffset);
}

int xuiInternalTextGraphemeClamp(const char* sText, int iLength, int iOffset)
{
	if ( sText == NULL ) return 0;
	if ( iLength < 0 ) iLength = (int)strlen(sText);
	return xuiInternalTextGraphemeClampRead(__xuiUnicodePointerRead,
		(void*)sText, iLength, iOffset);
}

static xui_word_break_property_t __xuiUnicodeWordProperty(uint32_t iCodepoint)
{
	int iLow;
	int iHigh;

	iLow = 0;
	iHigh = (int)(sizeof(__xuiUnicodeWordRanges) / sizeof(__xuiUnicodeWordRanges[0])) - 1;
	while ( iLow <= iHigh ) {
		int iMiddle = iLow + (iHigh - iLow) / 2;
		const xui_unicode_word_range_t* pRange = &__xuiUnicodeWordRanges[iMiddle];
		if ( iCodepoint < pRange->iFirst ) {
			iHigh = iMiddle - 1;
		} else if ( iCodepoint > pRange->iLast ) {
			iLow = iMiddle + 1;
		} else {
			return (xui_word_break_property_t)pRange->iProperty;
		}
	}
	return XUI_WB_OTHER;
}

static int __xuiUnicodeXidContinue(uint32_t iCodepoint)
{
	int iLow;
	int iHigh;

	iLow = 0;
	iHigh = (int)(sizeof(__xuiUnicodeXidContinueRanges) /
		sizeof(__xuiUnicodeXidContinueRanges[0])) - 1;
	while ( iLow <= iHigh ) {
		int iMiddle = iLow + (iHigh - iLow) / 2;
		const xui_unicode_binary_range_t* pRange = &__xuiUnicodeXidContinueRanges[iMiddle];
		if ( iCodepoint < pRange->iFirst ) {
			iHigh = iMiddle - 1;
		} else if ( iCodepoint > pRange->iLast ) {
			iLow = iMiddle + 1;
		} else {
			return 1;
		}
	}
	return 0;
}

static int __xuiUnicodeWordUnitRead(xui_internal_text_read_proc onRead, void* pUser,
	int iLength, int iOffset, xui_unicode_word_unit_t* pUnit)
{
	uint32_t iCodepoint;
	int iScalarEnd;

	if ( onRead == NULL || pUnit == NULL || iOffset < 0 || iOffset >= iLength ) return 0;
	if ( !__xuiUnicodeDecode(onRead, pUser, iLength, iOffset, &iCodepoint, &iScalarEnd) ) return 0;
	pUnit->iStart = iOffset;
	pUnit->iEnd = xuiInternalTextGraphemeNextRead(onRead, pUser, iLength, iOffset);
	if ( pUnit->iEnd <= iOffset ) pUnit->iEnd = iScalarEnd;
	pUnit->iCodepoint = iCodepoint;
	pUnit->iProperty = __xuiUnicodeWordProperty(iCodepoint);
	pUnit->bXidContinue = __xuiUnicodeXidContinue(iCodepoint);
	return 1;
}

static int __xuiUnicodeWordUnitBefore(xui_internal_text_read_proc onRead, void* pUser,
	int iLength, int iOffset, xui_unicode_word_unit_t* pUnit)
{
	int iStart;
	if ( iOffset <= 0 ) return 0;
	iStart = xuiInternalTextGraphemePrevRead(onRead, pUser, iLength, iOffset);
	return __xuiUnicodeWordUnitRead(onRead, pUser, iLength, iStart, pUnit);
}

static int __xuiUnicodeWordIgnored(xui_word_break_property_t iProperty)
{
	return iProperty == XUI_WB_EXTEND || iProperty == XUI_WB_FORMAT || iProperty == XUI_WB_ZWJ;
}

static int __xuiUnicodeWordNewline(xui_word_break_property_t iProperty)
{
	return iProperty == XUI_WB_CR || iProperty == XUI_WB_LF || iProperty == XUI_WB_NEWLINE;
}

static int __xuiUnicodeWordAhLetter(xui_word_break_property_t iProperty)
{
	return iProperty == XUI_WB_ALETTER || iProperty == XUI_WB_HEBREW_LETTER;
}

static int __xuiUnicodeWordMidLetter(xui_word_break_property_t iProperty)
{
	return iProperty == XUI_WB_MID_LETTER || iProperty == XUI_WB_MID_NUM_LET ||
		iProperty == XUI_WB_SINGLE_QUOTE;
}

static int __xuiUnicodeWordMidNumeric(xui_word_break_property_t iProperty)
{
	return iProperty == XUI_WB_MID_NUM || iProperty == XUI_WB_MID_NUM_LET ||
		iProperty == XUI_WB_SINGLE_QUOTE;
}

static int __xuiUnicodeWordWhitespace(uint32_t iCodepoint)
{
	return (iCodepoint >= 0x0009u && iCodepoint <= 0x000Du) || iCodepoint == 0x0020u ||
		iCodepoint == 0x0085u || iCodepoint == 0x00A0u || iCodepoint == 0x1680u ||
		(iCodepoint >= 0x2000u && iCodepoint <= 0x200Au) || iCodepoint == 0x2028u ||
		iCodepoint == 0x2029u || iCodepoint == 0x202Fu || iCodepoint == 0x205Fu ||
		iCodepoint == 0x3000u;
}

static int __xuiUnicodeWordTerminalExtra(uint32_t iCodepoint)
{
	return iCodepoint == '-' || iCodepoint == '.' || iCodepoint == '/' || iCodepoint == '\\' ||
		iCodepoint == ':' || iCodepoint == '@';
}

static int __xuiUnicodeWordPolicyUnit(const xui_unicode_word_unit_t* pUnit,
	xui_internal_word_policy_t iPolicy)
{
	if ( pUnit == NULL ) return 0;
	if ( iPolicy == XUI_INTERNAL_WORD_IDENTIFIER ) return pUnit->bXidContinue;
	if ( iPolicy == XUI_INTERNAL_WORD_TERMINAL ) {
		return pUnit->bXidContinue || __xuiUnicodeWordTerminalExtra(pUnit->iCodepoint);
	}
	return __xuiUnicodeWordAhLetter(pUnit->iProperty) ||
		pUnit->iProperty == XUI_WB_NUMERIC || pUnit->iProperty == XUI_WB_KATAKANA ||
		pUnit->iProperty == XUI_WB_EXTEND_NUM_LET || pUnit->bXidContinue;
}

static int __xuiUnicodeWordPreviousSignificant(xui_internal_text_read_proc onRead,
	void* pUser, int iLength, int iOffset, xui_unicode_word_unit_t* pUnit)
{
	xui_unicode_word_unit_t tUnit;
	while ( __xuiUnicodeWordUnitBefore(onRead, pUser, iLength, iOffset, &tUnit) ) {
		if ( !__xuiUnicodeWordIgnored(tUnit.iProperty) ) {
			if ( pUnit != NULL ) *pUnit = tUnit;
			return 1;
		}
		iOffset = tUnit.iStart;
	}
	return 0;
}

static int __xuiUnicodeWordNextSignificant(xui_internal_text_read_proc onRead,
	void* pUser, int iLength, int iOffset, xui_unicode_word_unit_t* pUnit)
{
	xui_unicode_word_unit_t tUnit;
	while ( iOffset < iLength && __xuiUnicodeWordUnitRead(onRead, pUser, iLength, iOffset, &tUnit) ) {
		if ( !__xuiUnicodeWordIgnored(tUnit.iProperty) ) {
			if ( pUnit != NULL ) *pUnit = tUnit;
			return 1;
		}
		if ( tUnit.iEnd <= iOffset ) break;
		iOffset = tUnit.iEnd;
	}
	return 0;
}

static int __xuiUnicodeWordNaturalBoundary(xui_internal_text_read_proc onRead,
	void* pUser, int iLength, int iOffset)
{
	xui_unicode_word_unit_t tImmediateLeft;
	xui_unicode_word_unit_t tImmediateRight;
	xui_unicode_word_unit_t tLeft;
	xui_unicode_word_unit_t tRight;
	xui_unicode_word_unit_t tOuter;
	int iRegionalCount;
	int iAt;

	if ( !__xuiUnicodeWordUnitBefore(onRead, pUser, iLength, iOffset, &tImmediateLeft) ||
	     !__xuiUnicodeWordUnitRead(onRead, pUser, iLength, iOffset, &tImmediateRight) ) return 1;
	if ( tImmediateLeft.iProperty == XUI_WB_CR && tImmediateRight.iProperty == XUI_WB_LF ) return 0;
	if ( __xuiUnicodeWordNewline(tImmediateLeft.iProperty) ||
	     __xuiUnicodeWordNewline(tImmediateRight.iProperty) ) return 1;
	if ( tImmediateLeft.iProperty == XUI_WB_WSEG_SPACE &&
	     tImmediateRight.iProperty == XUI_WB_WSEG_SPACE ) return 0;
	if ( __xuiUnicodeWordIgnored(tImmediateRight.iProperty) ) return 0;
	if ( !__xuiUnicodeWordPreviousSignificant(onRead, pUser, iLength, iOffset, &tLeft) ||
	     !__xuiUnicodeWordNextSignificant(onRead, pUser, iLength, iOffset, &tRight) ) return 1;
	if ( __xuiUnicodeWordNewline(tLeft.iProperty) || __xuiUnicodeWordNewline(tRight.iProperty) ) return 1;
	if ( __xuiUnicodeWordAhLetter(tLeft.iProperty) && __xuiUnicodeWordAhLetter(tRight.iProperty) ) return 0;
	if ( __xuiUnicodeWordAhLetter(tLeft.iProperty) && __xuiUnicodeWordMidLetter(tRight.iProperty) &&
	     __xuiUnicodeWordNextSignificant(onRead, pUser, iLength, tRight.iEnd, &tOuter) &&
	     __xuiUnicodeWordAhLetter(tOuter.iProperty) ) return 0;
	if ( __xuiUnicodeWordMidLetter(tLeft.iProperty) && __xuiUnicodeWordAhLetter(tRight.iProperty) &&
	     __xuiUnicodeWordPreviousSignificant(onRead, pUser, iLength, tLeft.iStart, &tOuter) &&
	     __xuiUnicodeWordAhLetter(tOuter.iProperty) ) return 0;
	if ( tLeft.iProperty == XUI_WB_HEBREW_LETTER && tRight.iProperty == XUI_WB_SINGLE_QUOTE ) return 0;
	if ( tLeft.iProperty == XUI_WB_HEBREW_LETTER && tRight.iProperty == XUI_WB_DOUBLE_QUOTE &&
	     __xuiUnicodeWordNextSignificant(onRead, pUser, iLength, tRight.iEnd, &tOuter) &&
	     tOuter.iProperty == XUI_WB_HEBREW_LETTER ) return 0;
	if ( tLeft.iProperty == XUI_WB_DOUBLE_QUOTE && tRight.iProperty == XUI_WB_HEBREW_LETTER &&
	     __xuiUnicodeWordPreviousSignificant(onRead, pUser, iLength, tLeft.iStart, &tOuter) &&
	     tOuter.iProperty == XUI_WB_HEBREW_LETTER ) return 0;
	if ( tLeft.iProperty == XUI_WB_NUMERIC && tRight.iProperty == XUI_WB_NUMERIC ) return 0;
	if ( __xuiUnicodeWordAhLetter(tLeft.iProperty) && tRight.iProperty == XUI_WB_NUMERIC ) return 0;
	if ( tLeft.iProperty == XUI_WB_NUMERIC && __xuiUnicodeWordAhLetter(tRight.iProperty) ) return 0;
	if ( tLeft.iProperty == XUI_WB_NUMERIC && __xuiUnicodeWordMidNumeric(tRight.iProperty) &&
	     __xuiUnicodeWordNextSignificant(onRead, pUser, iLength, tRight.iEnd, &tOuter) &&
	     tOuter.iProperty == XUI_WB_NUMERIC ) return 0;
	if ( __xuiUnicodeWordMidNumeric(tLeft.iProperty) && tRight.iProperty == XUI_WB_NUMERIC &&
	     __xuiUnicodeWordPreviousSignificant(onRead, pUser, iLength, tLeft.iStart, &tOuter) &&
	     tOuter.iProperty == XUI_WB_NUMERIC ) return 0;
	if ( tLeft.iProperty == XUI_WB_KATAKANA && tRight.iProperty == XUI_WB_KATAKANA ) return 0;
	if ( (__xuiUnicodeWordAhLetter(tLeft.iProperty) || tLeft.iProperty == XUI_WB_NUMERIC ||
	      tLeft.iProperty == XUI_WB_KATAKANA || tLeft.iProperty == XUI_WB_EXTEND_NUM_LET) &&
	     tRight.iProperty == XUI_WB_EXTEND_NUM_LET ) return 0;
	if ( tLeft.iProperty == XUI_WB_EXTEND_NUM_LET &&
	     (__xuiUnicodeWordAhLetter(tRight.iProperty) || tRight.iProperty == XUI_WB_NUMERIC ||
	      tRight.iProperty == XUI_WB_KATAKANA) ) return 0;
	if ( tLeft.iProperty != XUI_WB_REGIONAL || tRight.iProperty != XUI_WB_REGIONAL ) return 1;
	iRegionalCount = 0;
	iAt = iOffset;
	while ( __xuiUnicodeWordPreviousSignificant(onRead, pUser, iLength, iAt, &tOuter) &&
	        tOuter.iProperty == XUI_WB_REGIONAL ) {
		iRegionalCount++;
		iAt = tOuter.iStart;
	}
	return (iRegionalCount & 1) == 0;
}

int xuiInternalTextWordBoundaryRead(xui_internal_text_read_proc onRead, void* pUser,
	int iLength, int iOffset, xui_internal_word_policy_t iPolicy)
{
	xui_unicode_word_unit_t tImmediateLeft;
	xui_unicode_word_unit_t tImmediateRight;
	xui_unicode_word_unit_t tLeft;
	xui_unicode_word_unit_t tRight;

	if ( onRead == NULL || iLength <= 0 || iOffset <= 0 || iOffset >= iLength ) return 1;
	iOffset = xuiInternalTextGraphemeClampRead(onRead, pUser, iLength, iOffset);
	if ( iOffset <= 0 || iOffset >= iLength ) return 1;
	if ( iPolicy == XUI_INTERNAL_WORD_NATURAL ) {
		return __xuiUnicodeWordNaturalBoundary(onRead, pUser, iLength, iOffset);
	}
	if ( !__xuiUnicodeWordUnitBefore(onRead, pUser, iLength, iOffset, &tImmediateLeft) ||
	     !__xuiUnicodeWordUnitRead(onRead, pUser, iLength, iOffset, &tImmediateRight) ) return 1;
	if ( tImmediateLeft.iProperty == XUI_WB_CR && tImmediateRight.iProperty == XUI_WB_LF ) return 0;
	if ( __xuiUnicodeWordNewline(tImmediateLeft.iProperty) ||
	     __xuiUnicodeWordNewline(tImmediateRight.iProperty) ||
	     __xuiUnicodeWordWhitespace(tImmediateLeft.iCodepoint) ||
	     __xuiUnicodeWordWhitespace(tImmediateRight.iCodepoint) ) return 1;
	if ( __xuiUnicodeWordIgnored(tImmediateRight.iProperty) ) return 0;
	if ( !__xuiUnicodeWordPreviousSignificant(onRead, pUser, iLength, iOffset, &tLeft) ||
	     !__xuiUnicodeWordNextSignificant(onRead, pUser, iLength, iOffset, &tRight) ) return 1;
	return !(__xuiUnicodeWordPolicyUnit(&tLeft, iPolicy) &&
		__xuiUnicodeWordPolicyUnit(&tRight, iPolicy));
}

xui_internal_word_kind_t xuiInternalTextWordRangeRead(xui_internal_text_read_proc onRead,
	void* pUser, int iLength, int iOffset, xui_internal_word_policy_t iPolicy,
	int* pStart, int* pEnd)
{
	xui_unicode_word_unit_t tUnit;
	xui_unicode_word_unit_t tRangeUnit;
	int iStart;
	int iEnd;
	int iAt;

	if ( pStart != NULL ) *pStart = 0;
	if ( pEnd != NULL ) *pEnd = 0;
	if ( onRead == NULL || iLength <= 0 ) return XUI_INTERNAL_WORD_SPACE;
	iOffset = xuiInternalTextGraphemeClampRead(onRead, pUser, iLength, iOffset);
	if ( iOffset >= iLength ) iOffset = xuiInternalTextGraphemePrevRead(onRead, pUser, iLength, iLength);
	if ( !__xuiUnicodeWordUnitRead(onRead, pUser, iLength, iOffset, &tUnit) ) return XUI_INTERNAL_WORD_SPACE;
	if ( __xuiUnicodeWordWhitespace(tUnit.iCodepoint) ) {
		if ( pStart != NULL ) *pStart = iOffset;
		if ( pEnd != NULL ) *pEnd = iOffset;
		return XUI_INTERNAL_WORD_SPACE;
	}
	iStart = tUnit.iStart;
	iEnd = tUnit.iEnd;
	while ( iStart > 0 && !xuiInternalTextWordBoundaryRead(onRead, pUser, iLength, iStart, iPolicy) ) {
		int iPrevious = xuiInternalTextGraphemePrevRead(onRead, pUser, iLength, iStart);
		if ( iPrevious >= iStart ) break;
		iStart = iPrevious;
	}
	while ( iEnd < iLength && !xuiInternalTextWordBoundaryRead(onRead, pUser, iLength, iEnd, iPolicy) ) {
		int iNext = xuiInternalTextGraphemeNextRead(onRead, pUser, iLength, iEnd);
		if ( iNext <= iEnd ) break;
		iEnd = iNext;
	}
	if ( pStart != NULL ) *pStart = iStart;
	if ( pEnd != NULL ) *pEnd = iEnd;
	if ( __xuiUnicodeWordPolicyUnit(&tUnit, iPolicy) ) return XUI_INTERNAL_WORD_TEXT;
	for ( iAt = iStart; iAt < iEnd; iAt = tRangeUnit.iEnd ) {
		if ( !__xuiUnicodeWordUnitRead(onRead, pUser, iLength, iAt, &tRangeUnit) ||
		     tRangeUnit.iEnd <= iAt ) break;
		if ( __xuiUnicodeWordPolicyUnit(&tRangeUnit, iPolicy) ) return XUI_INTERNAL_WORD_TEXT;
	}
	return XUI_INTERNAL_WORD_SYMBOL;
}

int xuiInternalTextWordPrevRead(xui_internal_text_read_proc onRead, void* pUser,
	int iLength, int iOffset, xui_internal_word_policy_t iPolicy)
{
	int iStart;
	int iEnd;
	int iProbe;
	xui_internal_word_kind_t iKind;

	if ( onRead == NULL || iLength <= 0 ) return 0;
	iOffset = xuiInternalTextGraphemeClampRead(onRead, pUser, iLength, iOffset);
	while ( iOffset > 0 ) {
		iProbe = xuiInternalTextGraphemePrevRead(onRead, pUser, iLength, iOffset);
		iKind = xuiInternalTextWordRangeRead(onRead, pUser, iLength, iProbe,
			iPolicy, &iStart, &iEnd);
		if ( iKind == XUI_INTERNAL_WORD_TEXT ) return iStart;
		if ( iKind == XUI_INTERNAL_WORD_SYMBOL && iStart < iOffset ) iOffset = iStart;
		else iOffset = iProbe;
	}
	return 0;
}

int xuiInternalTextWordNextRead(xui_internal_text_read_proc onRead, void* pUser,
	int iLength, int iOffset, xui_internal_word_policy_t iPolicy)
{
	int iStart;
	int iEnd;
	int iNext;
	xui_internal_word_kind_t iKind;

	if ( onRead == NULL || iLength <= 0 ) return 0;
	iOffset = xuiInternalTextGraphemeClampRead(onRead, pUser, iLength, iOffset);
	if ( iOffset >= iLength ) return iLength;
	iKind = xuiInternalTextWordRangeRead(onRead, pUser, iLength, iOffset,
		iPolicy, &iStart, &iEnd);
	if ( iKind == XUI_INTERNAL_WORD_SPACE || iEnd <= iOffset ) {
		iOffset = xuiInternalTextGraphemeNextRead(onRead, pUser, iLength, iOffset);
	} else {
		iOffset = iEnd;
	}
	while ( iOffset < iLength ) {
		iKind = xuiInternalTextWordRangeRead(onRead, pUser, iLength, iOffset,
			iPolicy, &iStart, &iEnd);
		if ( iKind == XUI_INTERNAL_WORD_TEXT ) return iStart;
		if ( iKind == XUI_INTERNAL_WORD_SYMBOL && iEnd > iOffset ) {
			iOffset = iEnd;
		} else {
			iNext = xuiInternalTextGraphemeNextRead(onRead, pUser, iLength, iOffset);
			if ( iNext <= iOffset ) break;
			iOffset = iNext;
		}
	}
	return iLength;
}

int xuiInternalTextWordBoundary(const char* sText, int iLength, int iOffset,
	xui_internal_word_policy_t iPolicy)
{
	if ( sText == NULL ) return 1;
	if ( iLength < 0 ) iLength = (int)strlen(sText);
	return xuiInternalTextWordBoundaryRead(__xuiUnicodePointerRead, (void*)sText,
		iLength, iOffset, iPolicy);
}

xui_internal_word_kind_t xuiInternalTextWordRange(const char* sText, int iLength,
	int iOffset, xui_internal_word_policy_t iPolicy, int* pStart, int* pEnd)
{
	if ( sText == NULL ) {
		if ( pStart != NULL ) *pStart = 0;
		if ( pEnd != NULL ) *pEnd = 0;
		return XUI_INTERNAL_WORD_SPACE;
	}
	if ( iLength < 0 ) iLength = (int)strlen(sText);
	return xuiInternalTextWordRangeRead(__xuiUnicodePointerRead, (void*)sText,
		iLength, iOffset, iPolicy, pStart, pEnd);
}

int xuiInternalTextWordPrev(const char* sText, int iLength, int iOffset,
	xui_internal_word_policy_t iPolicy)
{
	if ( sText == NULL ) return 0;
	if ( iLength < 0 ) iLength = (int)strlen(sText);
	return xuiInternalTextWordPrevRead(__xuiUnicodePointerRead, (void*)sText,
		iLength, iOffset, iPolicy);
}

int xuiInternalTextWordNext(const char* sText, int iLength, int iOffset,
	xui_internal_word_policy_t iPolicy)
{
	if ( sText == NULL ) return 0;
	if ( iLength < 0 ) iLength = (int)strlen(sText);
	return xuiInternalTextWordNextRead(__xuiUnicodePointerRead, (void*)sText,
		iLength, iOffset, iPolicy);
}
