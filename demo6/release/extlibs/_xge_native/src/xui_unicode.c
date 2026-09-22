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

static int __xuiUnicodeRange(uint32_t iCodepoint, uint32_t iFirst, uint32_t iLast)
{
	return iCodepoint >= iFirst && iCodepoint <= iLast;
}

static int __xuiUnicodeIsExtend(uint32_t iCodepoint)
{
	return
		__xuiUnicodeRange(iCodepoint, 0x0300u, 0x036Fu) ||
		__xuiUnicodeRange(iCodepoint, 0x0483u, 0x0489u) ||
		__xuiUnicodeRange(iCodepoint, 0x0591u, 0x05BDu) ||
		iCodepoint == 0x05BFu ||
		__xuiUnicodeRange(iCodepoint, 0x05C1u, 0x05C2u) ||
		__xuiUnicodeRange(iCodepoint, 0x05C4u, 0x05C5u) ||
		iCodepoint == 0x05C7u ||
		__xuiUnicodeRange(iCodepoint, 0x0610u, 0x061Au) ||
		__xuiUnicodeRange(iCodepoint, 0x064Bu, 0x065Fu) ||
		iCodepoint == 0x0670u ||
		__xuiUnicodeRange(iCodepoint, 0x06D6u, 0x06DCu) ||
		__xuiUnicodeRange(iCodepoint, 0x06DFu, 0x06E4u) ||
		__xuiUnicodeRange(iCodepoint, 0x06E7u, 0x06E8u) ||
		__xuiUnicodeRange(iCodepoint, 0x06EAu, 0x06EDu) ||
		__xuiUnicodeRange(iCodepoint, 0x0711u, 0x0711u) ||
		__xuiUnicodeRange(iCodepoint, 0x0730u, 0x074Au) ||
		__xuiUnicodeRange(iCodepoint, 0x07A6u, 0x07B0u) ||
		__xuiUnicodeRange(iCodepoint, 0x07EBu, 0x07F3u) ||
		__xuiUnicodeRange(iCodepoint, 0x0816u, 0x082Du) ||
		__xuiUnicodeRange(iCodepoint, 0x0859u, 0x085Bu) ||
		__xuiUnicodeRange(iCodepoint, 0x08D3u, 0x0902u) ||
		__xuiUnicodeRange(iCodepoint, 0x093Au, 0x093Cu) ||
		__xuiUnicodeRange(iCodepoint, 0x0941u, 0x0948u) ||
		iCodepoint == 0x094Du ||
		__xuiUnicodeRange(iCodepoint, 0x0951u, 0x0957u) ||
		__xuiUnicodeRange(iCodepoint, 0x0962u, 0x0963u) ||
		__xuiUnicodeRange(iCodepoint, 0x0981u, 0x0981u) ||
		iCodepoint == 0x09BCu ||
		__xuiUnicodeRange(iCodepoint, 0x09C1u, 0x09C4u) ||
		iCodepoint == 0x09CDu ||
		__xuiUnicodeRange(iCodepoint, 0x09E2u, 0x09E3u) ||
		__xuiUnicodeRange(iCodepoint, 0x0A01u, 0x0A02u) ||
		iCodepoint == 0x0A3Cu ||
		__xuiUnicodeRange(iCodepoint, 0x0A41u, 0x0A42u) ||
		__xuiUnicodeRange(iCodepoint, 0x0A47u, 0x0A48u) ||
		__xuiUnicodeRange(iCodepoint, 0x0A4Bu, 0x0A4Du) ||
		__xuiUnicodeRange(iCodepoint, 0x0A51u, 0x0A51u) ||
		__xuiUnicodeRange(iCodepoint, 0x0A70u, 0x0A71u) ||
		iCodepoint == 0x0A75u ||
		__xuiUnicodeRange(iCodepoint, 0x0A81u, 0x0A82u) ||
		iCodepoint == 0x0ABCu ||
		__xuiUnicodeRange(iCodepoint, 0x0AC1u, 0x0AC8u) ||
		iCodepoint == 0x0ACDu ||
		__xuiUnicodeRange(iCodepoint, 0x0AE2u, 0x0AE3u) ||
		__xuiUnicodeRange(iCodepoint, 0x0B01u, 0x0B01u) ||
		iCodepoint == 0x0B3Cu ||
		iCodepoint == 0x0B3Fu ||
		__xuiUnicodeRange(iCodepoint, 0x0B41u, 0x0B44u) ||
		iCodepoint == 0x0B4Du ||
		__xuiUnicodeRange(iCodepoint, 0x0B56u, 0x0B56u) ||
		__xuiUnicodeRange(iCodepoint, 0x0B62u, 0x0B63u) ||
		iCodepoint == 0x0B82u ||
		iCodepoint == 0x0BC0u ||
		iCodepoint == 0x0BCDu ||
		iCodepoint == 0x0C00u ||
		__xuiUnicodeRange(iCodepoint, 0x0C3Eu, 0x0C40u) ||
		__xuiUnicodeRange(iCodepoint, 0x0C46u, 0x0C48u) ||
		__xuiUnicodeRange(iCodepoint, 0x0C4Au, 0x0C4Du) ||
		__xuiUnicodeRange(iCodepoint, 0x0C55u, 0x0C56u) ||
		__xuiUnicodeRange(iCodepoint, 0x0C62u, 0x0C63u) ||
		iCodepoint == 0x0C81u ||
		iCodepoint == 0x0CBCu ||
		iCodepoint == 0x0CBFu ||
		iCodepoint == 0x0CC6u ||
		__xuiUnicodeRange(iCodepoint, 0x0CCCu, 0x0CCDu) ||
		__xuiUnicodeRange(iCodepoint, 0x0CE2u, 0x0CE3u) ||
		__xuiUnicodeRange(iCodepoint, 0x0D00u, 0x0D01u) ||
		__xuiUnicodeRange(iCodepoint, 0x0D3Bu, 0x0D3Cu) ||
		__xuiUnicodeRange(iCodepoint, 0x0D41u, 0x0D44u) ||
		iCodepoint == 0x0D4Du ||
		__xuiUnicodeRange(iCodepoint, 0x0D62u, 0x0D63u) ||
		iCodepoint == 0x0DCAu ||
		__xuiUnicodeRange(iCodepoint, 0x0DD2u, 0x0DD4u) ||
		iCodepoint == 0x0DD6u ||
		iCodepoint == 0x0E31u ||
		__xuiUnicodeRange(iCodepoint, 0x0E34u, 0x0E3Au) ||
		__xuiUnicodeRange(iCodepoint, 0x0E47u, 0x0E4Eu) ||
		iCodepoint == 0x0EB1u ||
		__xuiUnicodeRange(iCodepoint, 0x0EB4u, 0x0EBCu) ||
		__xuiUnicodeRange(iCodepoint, 0x0EC8u, 0x0ECDu) ||
		__xuiUnicodeRange(iCodepoint, 0x0F18u, 0x0F19u) ||
		iCodepoint == 0x0F35u ||
		iCodepoint == 0x0F37u ||
		iCodepoint == 0x0F39u ||
		__xuiUnicodeRange(iCodepoint, 0x0F71u, 0x0F84u) ||
		__xuiUnicodeRange(iCodepoint, 0x0F86u, 0x0F87u) ||
		__xuiUnicodeRange(iCodepoint, 0x0F8Du, 0x0FBCu) ||
		iCodepoint == 0x0FC6u ||
		__xuiUnicodeRange(iCodepoint, 0x102Du, 0x1030u) ||
		__xuiUnicodeRange(iCodepoint, 0x1032u, 0x1037u) ||
		__xuiUnicodeRange(iCodepoint, 0x1039u, 0x103Au) ||
		__xuiUnicodeRange(iCodepoint, 0x103Du, 0x103Eu) ||
		__xuiUnicodeRange(iCodepoint, 0x1058u, 0x1059u) ||
		__xuiUnicodeRange(iCodepoint, 0x105Eu, 0x1060u) ||
		__xuiUnicodeRange(iCodepoint, 0x1071u, 0x1074u) ||
		iCodepoint == 0x1082u ||
		__xuiUnicodeRange(iCodepoint, 0x1085u, 0x1086u) ||
		iCodepoint == 0x108Du ||
		iCodepoint == 0x109Du ||
		__xuiUnicodeRange(iCodepoint, 0x135Du, 0x135Fu) ||
		__xuiUnicodeRange(iCodepoint, 0x1712u, 0x1714u) ||
		__xuiUnicodeRange(iCodepoint, 0x1732u, 0x1734u) ||
		__xuiUnicodeRange(iCodepoint, 0x1752u, 0x1753u) ||
		__xuiUnicodeRange(iCodepoint, 0x1772u, 0x1773u) ||
		__xuiUnicodeRange(iCodepoint, 0x17B4u, 0x17B5u) ||
		__xuiUnicodeRange(iCodepoint, 0x17B7u, 0x17BDu) ||
		iCodepoint == 0x17C6u ||
		__xuiUnicodeRange(iCodepoint, 0x17C9u, 0x17D3u) ||
		iCodepoint == 0x17DDu ||
		__xuiUnicodeRange(iCodepoint, 0x180Bu, 0x180Du) ||
		iCodepoint == 0x180Fu ||
		__xuiUnicodeRange(iCodepoint, 0x1885u, 0x1886u) ||
		iCodepoint == 0x18A9u ||
		__xuiUnicodeRange(iCodepoint, 0x1920u, 0x1922u) ||
		__xuiUnicodeRange(iCodepoint, 0x1927u, 0x1928u) ||
		iCodepoint == 0x1932u ||
		__xuiUnicodeRange(iCodepoint, 0x1939u, 0x193Bu) ||
		__xuiUnicodeRange(iCodepoint, 0x1A17u, 0x1A18u) ||
		iCodepoint == 0x1A1Bu ||
		iCodepoint == 0x1A56u ||
		__xuiUnicodeRange(iCodepoint, 0x1A58u, 0x1A5Eu) ||
		iCodepoint == 0x1A60u ||
		iCodepoint == 0x1A62u ||
		__xuiUnicodeRange(iCodepoint, 0x1A65u, 0x1A6Cu) ||
		__xuiUnicodeRange(iCodepoint, 0x1A73u, 0x1A7Cu) ||
		iCodepoint == 0x1A7Fu ||
		__xuiUnicodeRange(iCodepoint, 0x1AB0u, 0x1ACEu) ||
		__xuiUnicodeRange(iCodepoint, 0x1B00u, 0x1B03u) ||
		iCodepoint == 0x1B34u ||
		__xuiUnicodeRange(iCodepoint, 0x1B36u, 0x1B3Au) ||
		iCodepoint == 0x1B3Cu ||
		iCodepoint == 0x1B42u ||
		__xuiUnicodeRange(iCodepoint, 0x1B6Bu, 0x1B73u) ||
		__xuiUnicodeRange(iCodepoint, 0x1B80u, 0x1B81u) ||
		__xuiUnicodeRange(iCodepoint, 0x1BA2u, 0x1BA5u) ||
		__xuiUnicodeRange(iCodepoint, 0x1BA8u, 0x1BA9u) ||
		__xuiUnicodeRange(iCodepoint, 0x1BABu, 0x1BADu) ||
		iCodepoint == 0x1BE6u ||
		__xuiUnicodeRange(iCodepoint, 0x1BE8u, 0x1BE9u) ||
		iCodepoint == 0x1BEDu ||
		__xuiUnicodeRange(iCodepoint, 0x1BEFu, 0x1BF1u) ||
		__xuiUnicodeRange(iCodepoint, 0x1C2Cu, 0x1C33u) ||
		__xuiUnicodeRange(iCodepoint, 0x1C36u, 0x1C37u) ||
		__xuiUnicodeRange(iCodepoint, 0x1CD0u, 0x1CD2u) ||
		__xuiUnicodeRange(iCodepoint, 0x1CD4u, 0x1CE0u) ||
		__xuiUnicodeRange(iCodepoint, 0x1CE2u, 0x1CE8u) ||
		iCodepoint == 0x1CEDu ||
		iCodepoint == 0x1CF4u ||
		__xuiUnicodeRange(iCodepoint, 0x1CF8u, 0x1CF9u) ||
		__xuiUnicodeRange(iCodepoint, 0x1DC0u, 0x1DFFu) ||
		__xuiUnicodeRange(iCodepoint, 0x20D0u, 0x20FFu) ||
		__xuiUnicodeRange(iCodepoint, 0x2CEFu, 0x2CF1u) ||
		iCodepoint == 0x2D7Fu ||
		__xuiUnicodeRange(iCodepoint, 0x2DE0u, 0x2DFFu) ||
		__xuiUnicodeRange(iCodepoint, 0x302Au, 0x302Du) ||
		__xuiUnicodeRange(iCodepoint, 0x3099u, 0x309Au) ||
		__xuiUnicodeRange(iCodepoint, 0xA66Fu, 0xA672u) ||
		__xuiUnicodeRange(iCodepoint, 0xA674u, 0xA67Du) ||
		__xuiUnicodeRange(iCodepoint, 0xA69Eu, 0xA69Fu) ||
		__xuiUnicodeRange(iCodepoint, 0xA6F0u, 0xA6F1u) ||
		iCodepoint == 0xA802u ||
		iCodepoint == 0xA806u ||
		iCodepoint == 0xA80Bu ||
		__xuiUnicodeRange(iCodepoint, 0xA825u, 0xA826u) ||
		iCodepoint == 0xA82Cu ||
		__xuiUnicodeRange(iCodepoint, 0xA8C4u, 0xA8C5u) ||
		__xuiUnicodeRange(iCodepoint, 0xA8E0u, 0xA8F1u) ||
		iCodepoint == 0xA8FFu ||
		__xuiUnicodeRange(iCodepoint, 0xA926u, 0xA92Du) ||
		__xuiUnicodeRange(iCodepoint, 0xA947u, 0xA951u) ||
		__xuiUnicodeRange(iCodepoint, 0xA980u, 0xA982u) ||
		iCodepoint == 0xA9B3u ||
		__xuiUnicodeRange(iCodepoint, 0xA9B6u, 0xA9B9u) ||
		iCodepoint == 0xA9BCu ||
		iCodepoint == 0xA9E5u ||
		__xuiUnicodeRange(iCodepoint, 0xAA29u, 0xAA2Eu) ||
		__xuiUnicodeRange(iCodepoint, 0xAA31u, 0xAA32u) ||
		__xuiUnicodeRange(iCodepoint, 0xAA35u, 0xAA36u) ||
		iCodepoint == 0xAA43u ||
		iCodepoint == 0xAA4Cu ||
		iCodepoint == 0xAA7Cu ||
		iCodepoint == 0xAAB0u ||
		__xuiUnicodeRange(iCodepoint, 0xAAB2u, 0xAAB4u) ||
		__xuiUnicodeRange(iCodepoint, 0xAAB7u, 0xAAB8u) ||
		__xuiUnicodeRange(iCodepoint, 0xAABEu, 0xAABFu) ||
		iCodepoint == 0xAAC1u ||
		__xuiUnicodeRange(iCodepoint, 0xAAECu, 0xAAEDu) ||
		iCodepoint == 0xAAF6u ||
		iCodepoint == 0xABE5u ||
		iCodepoint == 0xABE8u ||
		iCodepoint == 0xABEDu ||
		__xuiUnicodeRange(iCodepoint, 0xFB1Eu, 0xFB1Eu) ||
		__xuiUnicodeRange(iCodepoint, 0xFE00u, 0xFE0Fu) ||
		__xuiUnicodeRange(iCodepoint, 0xFE20u, 0xFE2Fu) ||
		iCodepoint == 0x101FDu ||
		iCodepoint == 0x102E0u ||
		__xuiUnicodeRange(iCodepoint, 0x10376u, 0x1037Au) ||
		__xuiUnicodeRange(iCodepoint, 0x10A01u, 0x10A03u) ||
		__xuiUnicodeRange(iCodepoint, 0x10A05u, 0x10A06u) ||
		__xuiUnicodeRange(iCodepoint, 0x10A0Cu, 0x10A0Fu) ||
		__xuiUnicodeRange(iCodepoint, 0x10A38u, 0x10A3Au) ||
		iCodepoint == 0x10A3Fu ||
		__xuiUnicodeRange(iCodepoint, 0x10AE5u, 0x10AE6u) ||
		__xuiUnicodeRange(iCodepoint, 0x10D24u, 0x10D27u) ||
		__xuiUnicodeRange(iCodepoint, 0x10EABu, 0x10EACu) ||
		__xuiUnicodeRange(iCodepoint, 0x10F46u, 0x10F50u) ||
		iCodepoint == 0x11001u ||
		__xuiUnicodeRange(iCodepoint, 0x11038u, 0x11046u) ||
		__xuiUnicodeRange(iCodepoint, 0x11070u, 0x11070u) ||
		__xuiUnicodeRange(iCodepoint, 0x11073u, 0x11074u) ||
		__xuiUnicodeRange(iCodepoint, 0x1107Fu, 0x11081u) ||
		__xuiUnicodeRange(iCodepoint, 0x110B3u, 0x110B6u) ||
		__xuiUnicodeRange(iCodepoint, 0x110B9u, 0x110BAu) ||
		__xuiUnicodeRange(iCodepoint, 0x11100u, 0x11102u) ||
		__xuiUnicodeRange(iCodepoint, 0x11127u, 0x1112Bu) ||
		__xuiUnicodeRange(iCodepoint, 0x1112Du, 0x11134u) ||
		iCodepoint == 0x11173u ||
		__xuiUnicodeRange(iCodepoint, 0x11180u, 0x11181u) ||
		__xuiUnicodeRange(iCodepoint, 0x111B6u, 0x111BEu) ||
		__xuiUnicodeRange(iCodepoint, 0x111C9u, 0x111CCu) ||
		iCodepoint == 0x111CFu ||
		__xuiUnicodeRange(iCodepoint, 0x1122Fu, 0x11231u) ||
		iCodepoint == 0x11234u ||
		__xuiUnicodeRange(iCodepoint, 0x11236u, 0x11237u) ||
		iCodepoint == 0x1123Eu ||
		iCodepoint == 0x112DFu ||
		__xuiUnicodeRange(iCodepoint, 0x112E3u, 0x112EAu) ||
		__xuiUnicodeRange(iCodepoint, 0x11300u, 0x11301u) ||
		__xuiUnicodeRange(iCodepoint, 0x1133Bu, 0x1133Cu) ||
		iCodepoint == 0x11340u ||
		__xuiUnicodeRange(iCodepoint, 0x11366u, 0x1136Cu) ||
		__xuiUnicodeRange(iCodepoint, 0x11370u, 0x11374u) ||
		__xuiUnicodeRange(iCodepoint, 0x11438u, 0x1143Fu) ||
		__xuiUnicodeRange(iCodepoint, 0x11442u, 0x11444u) ||
		iCodepoint == 0x11446u ||
		iCodepoint == 0x1145Eu ||
		__xuiUnicodeRange(iCodepoint, 0x114B3u, 0x114B8u) ||
		iCodepoint == 0x114BAu ||
		__xuiUnicodeRange(iCodepoint, 0x114BFu, 0x114C0u) ||
		__xuiUnicodeRange(iCodepoint, 0x114C2u, 0x114C3u) ||
		__xuiUnicodeRange(iCodepoint, 0x115B2u, 0x115B5u) ||
		__xuiUnicodeRange(iCodepoint, 0x115BCu, 0x115BDu) ||
		__xuiUnicodeRange(iCodepoint, 0x115BFu, 0x115C0u) ||
		__xuiUnicodeRange(iCodepoint, 0x115DCu, 0x115DDu) ||
		__xuiUnicodeRange(iCodepoint, 0x11633u, 0x1163Au) ||
		iCodepoint == 0x1163Du ||
		__xuiUnicodeRange(iCodepoint, 0x1163Fu, 0x11640u) ||
		iCodepoint == 0x116ABu ||
		iCodepoint == 0x116ADu ||
		__xuiUnicodeRange(iCodepoint, 0x116B0u, 0x116B5u) ||
		iCodepoint == 0x116B7u ||
		__xuiUnicodeRange(iCodepoint, 0x1171Du, 0x1171Fu) ||
		__xuiUnicodeRange(iCodepoint, 0x11722u, 0x11725u) ||
		__xuiUnicodeRange(iCodepoint, 0x11727u, 0x1172Bu) ||
		__xuiUnicodeRange(iCodepoint, 0x1182Fu, 0x11837u) ||
		__xuiUnicodeRange(iCodepoint, 0x11839u, 0x1183Au) ||
		__xuiUnicodeRange(iCodepoint, 0x1193Bu, 0x1193Cu) ||
		iCodepoint == 0x1193Eu ||
		iCodepoint == 0x11943u ||
		__xuiUnicodeRange(iCodepoint, 0x119D4u, 0x119D7u) ||
		__xuiUnicodeRange(iCodepoint, 0x119DAu, 0x119DBu) ||
		iCodepoint == 0x119E0u ||
		__xuiUnicodeRange(iCodepoint, 0x11A01u, 0x11A0Au) ||
		__xuiUnicodeRange(iCodepoint, 0x11A33u, 0x11A38u) ||
		__xuiUnicodeRange(iCodepoint, 0x11A3Bu, 0x11A3Eu) ||
		iCodepoint == 0x11A47u ||
		__xuiUnicodeRange(iCodepoint, 0x11A51u, 0x11A56u) ||
		__xuiUnicodeRange(iCodepoint, 0x11A59u, 0x11A5Bu) ||
		__xuiUnicodeRange(iCodepoint, 0x11A8Au, 0x11A96u) ||
		__xuiUnicodeRange(iCodepoint, 0x11A98u, 0x11A99u) ||
		__xuiUnicodeRange(iCodepoint, 0x11C30u, 0x11C36u) ||
		__xuiUnicodeRange(iCodepoint, 0x11C38u, 0x11C3Du) ||
		iCodepoint == 0x11C3Fu ||
		__xuiUnicodeRange(iCodepoint, 0x11C92u, 0x11CA7u) ||
		__xuiUnicodeRange(iCodepoint, 0x11CAAu, 0x11CB0u) ||
		__xuiUnicodeRange(iCodepoint, 0x11CB2u, 0x11CB3u) ||
		__xuiUnicodeRange(iCodepoint, 0x11CB5u, 0x11CB6u) ||
		__xuiUnicodeRange(iCodepoint, 0x11D31u, 0x11D36u) ||
		iCodepoint == 0x11D3Au ||
		__xuiUnicodeRange(iCodepoint, 0x11D3Cu, 0x11D3Du) ||
		__xuiUnicodeRange(iCodepoint, 0x11D3Fu, 0x11D45u) ||
		iCodepoint == 0x11D47u ||
		__xuiUnicodeRange(iCodepoint, 0x11D90u, 0x11D91u) ||
		iCodepoint == 0x11D95u ||
		iCodepoint == 0x11D97u ||
		__xuiUnicodeRange(iCodepoint, 0x11EF3u, 0x11EF4u) ||
		__xuiUnicodeRange(iCodepoint, 0x13430u, 0x13438u) ||
		__xuiUnicodeRange(iCodepoint, 0x16AF0u, 0x16AF4u) ||
		__xuiUnicodeRange(iCodepoint, 0x16B30u, 0x16B36u) ||
		__xuiUnicodeRange(iCodepoint, 0x16F4Fu, 0x16F4Fu) ||
		__xuiUnicodeRange(iCodepoint, 0x16F8Fu, 0x16F92u) ||
		__xuiUnicodeRange(iCodepoint, 0x1BC9Du, 0x1BC9Eu) ||
		__xuiUnicodeRange(iCodepoint, 0x1CF00u, 0x1CF46u) ||
		__xuiUnicodeRange(iCodepoint, 0x1D167u, 0x1D169u) ||
		__xuiUnicodeRange(iCodepoint, 0x1D17Bu, 0x1D182u) ||
		__xuiUnicodeRange(iCodepoint, 0x1D185u, 0x1D18Bu) ||
		__xuiUnicodeRange(iCodepoint, 0x1D1AAu, 0x1D1ADu) ||
		__xuiUnicodeRange(iCodepoint, 0x1D242u, 0x1D244u) ||
		__xuiUnicodeRange(iCodepoint, 0x1DA00u, 0x1DA36u) ||
		__xuiUnicodeRange(iCodepoint, 0x1DA3Bu, 0x1DA6Cu) ||
		iCodepoint == 0x1DA75u ||
		iCodepoint == 0x1DA84u ||
		__xuiUnicodeRange(iCodepoint, 0x1DA9Bu, 0x1DA9Fu) ||
		__xuiUnicodeRange(iCodepoint, 0x1DAA1u, 0x1DAAFu) ||
		__xuiUnicodeRange(iCodepoint, 0x1E000u, 0x1E006u) ||
		__xuiUnicodeRange(iCodepoint, 0x1E008u, 0x1E018u) ||
		__xuiUnicodeRange(iCodepoint, 0x1E01Bu, 0x1E021u) ||
		__xuiUnicodeRange(iCodepoint, 0x1E023u, 0x1E024u) ||
		__xuiUnicodeRange(iCodepoint, 0x1E026u, 0x1E02Au) ||
		__xuiUnicodeRange(iCodepoint, 0x1E130u, 0x1E136u) ||
		iCodepoint == 0x1E2AEu ||
		__xuiUnicodeRange(iCodepoint, 0x1E2ECu, 0x1E2EFu) ||
		__xuiUnicodeRange(iCodepoint, 0x1E8D0u, 0x1E8D6u) ||
		__xuiUnicodeRange(iCodepoint, 0x1E944u, 0x1E94Au) ||
		__xuiUnicodeRange(iCodepoint, 0x1F3FBu, 0x1F3FFu) ||
		__xuiUnicodeRange(iCodepoint, 0xE0020u, 0xE007Fu) ||
		__xuiUnicodeRange(iCodepoint, 0xE0100u, 0xE01EFu);
}

static int __xuiUnicodeIsSpacingMark(uint32_t iCodepoint)
{
	return
		iCodepoint == 0x0903u ||
		__xuiUnicodeRange(iCodepoint, 0x093Bu, 0x0940u) ||
		__xuiUnicodeRange(iCodepoint, 0x0949u, 0x094Cu) ||
		__xuiUnicodeRange(iCodepoint, 0x0982u, 0x0983u) ||
		__xuiUnicodeRange(iCodepoint, 0x09BEu, 0x09C0u) ||
		__xuiUnicodeRange(iCodepoint, 0x09C7u, 0x09C8u) ||
		__xuiUnicodeRange(iCodepoint, 0x09CBu, 0x09CCu) ||
		__xuiUnicodeRange(iCodepoint, 0x0A3Eu, 0x0A40u) ||
		__xuiUnicodeRange(iCodepoint, 0x0A83u, 0x0A83u) ||
		__xuiUnicodeRange(iCodepoint, 0x0ABEu, 0x0AC0u) ||
		__xuiUnicodeRange(iCodepoint, 0x0AC9u, 0x0AC9u) ||
		__xuiUnicodeRange(iCodepoint, 0x0ACBu, 0x0ACCu) ||
		__xuiUnicodeRange(iCodepoint, 0x0B02u, 0x0B03u) ||
		iCodepoint == 0x0B3Eu ||
		iCodepoint == 0x0B40u ||
		__xuiUnicodeRange(iCodepoint, 0x0B47u, 0x0B48u) ||
		__xuiUnicodeRange(iCodepoint, 0x0B4Bu, 0x0B4Cu) ||
		__xuiUnicodeRange(iCodepoint, 0x0BBEu, 0x0BBFu) ||
		__xuiUnicodeRange(iCodepoint, 0x0BC1u, 0x0BC2u) ||
		__xuiUnicodeRange(iCodepoint, 0x0BC6u, 0x0BC8u) ||
		__xuiUnicodeRange(iCodepoint, 0x0BCAu, 0x0BCCu) ||
		__xuiUnicodeRange(iCodepoint, 0x0C01u, 0x0C03u) ||
		__xuiUnicodeRange(iCodepoint, 0x0C41u, 0x0C44u) ||
		__xuiUnicodeRange(iCodepoint, 0x0C82u, 0x0C83u) ||
		iCodepoint == 0x0CBEu ||
		__xuiUnicodeRange(iCodepoint, 0x0CC0u, 0x0CC4u) ||
		__xuiUnicodeRange(iCodepoint, 0x0CC7u, 0x0CC8u) ||
		__xuiUnicodeRange(iCodepoint, 0x0CCAu, 0x0CCBu) ||
		__xuiUnicodeRange(iCodepoint, 0x0D02u, 0x0D03u) ||
		__xuiUnicodeRange(iCodepoint, 0x0D3Eu, 0x0D40u) ||
		__xuiUnicodeRange(iCodepoint, 0x0D46u, 0x0D48u) ||
		__xuiUnicodeRange(iCodepoint, 0x0D4Au, 0x0D4Cu) ||
		__xuiUnicodeRange(iCodepoint, 0x0D82u, 0x0D83u) ||
		__xuiUnicodeRange(iCodepoint, 0x0DD0u, 0x0DD1u) ||
		__xuiUnicodeRange(iCodepoint, 0x0DD8u, 0x0DDFu) ||
		__xuiUnicodeRange(iCodepoint, 0x0DF2u, 0x0DF3u) ||
		__xuiUnicodeRange(iCodepoint, 0x0F3Eu, 0x0F3Fu) ||
		iCodepoint == 0x0F7Fu ||
		__xuiUnicodeRange(iCodepoint, 0x102Bu, 0x102Cu) ||
		iCodepoint == 0x1031u ||
		iCodepoint == 0x1038u ||
		__xuiUnicodeRange(iCodepoint, 0x103Bu, 0x103Cu) ||
		__xuiUnicodeRange(iCodepoint, 0x1056u, 0x1057u) ||
		__xuiUnicodeRange(iCodepoint, 0x1062u, 0x1064u) ||
		__xuiUnicodeRange(iCodepoint, 0x1067u, 0x106Du) ||
		iCodepoint == 0x1083u ||
		__xuiUnicodeRange(iCodepoint, 0x1087u, 0x108Cu) ||
		iCodepoint == 0x108Fu ||
		__xuiUnicodeRange(iCodepoint, 0x109Au, 0x109Cu) ||
		iCodepoint == 0x17B6u ||
		__xuiUnicodeRange(iCodepoint, 0x17BEu, 0x17C5u) ||
		__xuiUnicodeRange(iCodepoint, 0x17C7u, 0x17C8u) ||
		__xuiUnicodeRange(iCodepoint, 0x1923u, 0x1926u) ||
		__xuiUnicodeRange(iCodepoint, 0x1929u, 0x192Bu) ||
		__xuiUnicodeRange(iCodepoint, 0x1930u, 0x1931u) ||
		__xuiUnicodeRange(iCodepoint, 0x1933u, 0x1938u) ||
		__xuiUnicodeRange(iCodepoint, 0x1A19u, 0x1A1Au) ||
		iCodepoint == 0x1A55u ||
		iCodepoint == 0x1A57u ||
		iCodepoint == 0x1A61u ||
		__xuiUnicodeRange(iCodepoint, 0x1A63u, 0x1A64u) ||
		__xuiUnicodeRange(iCodepoint, 0x1A6Du, 0x1A72u) ||
		iCodepoint == 0x1B04u ||
		iCodepoint == 0x1B35u ||
		iCodepoint == 0x1B3Bu ||
		__xuiUnicodeRange(iCodepoint, 0x1B3Du, 0x1B41u) ||
		__xuiUnicodeRange(iCodepoint, 0x1B43u, 0x1B44u) ||
		iCodepoint == 0x1B82u ||
		iCodepoint == 0x1BA1u ||
		__xuiUnicodeRange(iCodepoint, 0x1BA6u, 0x1BA7u) ||
		iCodepoint == 0x1BAAu ||
		iCodepoint == 0x1BE7u ||
		__xuiUnicodeRange(iCodepoint, 0x1BEAu, 0x1BECu) ||
		iCodepoint == 0x1BEEu ||
		__xuiUnicodeRange(iCodepoint, 0x1BF2u, 0x1BF3u) ||
		__xuiUnicodeRange(iCodepoint, 0x1C24u, 0x1C2Bu) ||
		__xuiUnicodeRange(iCodepoint, 0x1C34u, 0x1C35u) ||
		__xuiUnicodeRange(iCodepoint, 0xA823u, 0xA824u) ||
		iCodepoint == 0xA827u ||
		__xuiUnicodeRange(iCodepoint, 0xA880u, 0xA881u) ||
		__xuiUnicodeRange(iCodepoint, 0xA8B4u, 0xA8C3u) ||
		__xuiUnicodeRange(iCodepoint, 0xA952u, 0xA953u) ||
		iCodepoint == 0xA983u ||
		__xuiUnicodeRange(iCodepoint, 0xA9B4u, 0xA9B5u) ||
		__xuiUnicodeRange(iCodepoint, 0xA9BAu, 0xA9BBu) ||
		__xuiUnicodeRange(iCodepoint, 0xA9BDu, 0xA9C0u) ||
		__xuiUnicodeRange(iCodepoint, 0xAA2Fu, 0xAA30u) ||
		__xuiUnicodeRange(iCodepoint, 0xAA33u, 0xAA34u) ||
		iCodepoint == 0xAA4Du ||
		iCodepoint == 0xAA7Bu ||
		iCodepoint == 0xAA7Du ||
		iCodepoint == 0xAAEBu ||
		__xuiUnicodeRange(iCodepoint, 0xAAEEu, 0xAAEFu) ||
		iCodepoint == 0xAAF5u ||
		__xuiUnicodeRange(iCodepoint, 0xABE3u, 0xABE4u) ||
		__xuiUnicodeRange(iCodepoint, 0xABE6u, 0xABE7u) ||
		__xuiUnicodeRange(iCodepoint, 0xABE9u, 0xABEAu) ||
		iCodepoint == 0xABECu ||
		iCodepoint == 0x11000u ||
		iCodepoint == 0x11002u ||
		iCodepoint == 0x11082u ||
		__xuiUnicodeRange(iCodepoint, 0x110B0u, 0x110B2u) ||
		__xuiUnicodeRange(iCodepoint, 0x110B7u, 0x110B8u) ||
		iCodepoint == 0x1112Cu ||
		__xuiUnicodeRange(iCodepoint, 0x11145u, 0x11146u) ||
		iCodepoint == 0x11182u ||
		__xuiUnicodeRange(iCodepoint, 0x111B3u, 0x111B5u) ||
		__xuiUnicodeRange(iCodepoint, 0x111BFu, 0x111C0u) ||
		__xuiUnicodeRange(iCodepoint, 0x1122Cu, 0x1122Eu) ||
		__xuiUnicodeRange(iCodepoint, 0x11232u, 0x11233u) ||
		iCodepoint == 0x11235u ||
		__xuiUnicodeRange(iCodepoint, 0x112E0u, 0x112E2u) ||
		__xuiUnicodeRange(iCodepoint, 0x11302u, 0x11303u) ||
		__xuiUnicodeRange(iCodepoint, 0x1133Eu, 0x1133Fu) ||
		__xuiUnicodeRange(iCodepoint, 0x11341u, 0x11344u) ||
		__xuiUnicodeRange(iCodepoint, 0x11347u, 0x11348u) ||
		__xuiUnicodeRange(iCodepoint, 0x1134Bu, 0x1134Du) ||
		iCodepoint == 0x11357u ||
		__xuiUnicodeRange(iCodepoint, 0x11435u, 0x11437u) ||
		__xuiUnicodeRange(iCodepoint, 0x11440u, 0x11441u) ||
		iCodepoint == 0x11445u ||
		__xuiUnicodeRange(iCodepoint, 0x114B0u, 0x114B2u) ||
		iCodepoint == 0x114B9u ||
		__xuiUnicodeRange(iCodepoint, 0x114BBu, 0x114BEu) ||
		iCodepoint == 0x114C1u ||
		__xuiUnicodeRange(iCodepoint, 0x115AFu, 0x115B1u) ||
		__xuiUnicodeRange(iCodepoint, 0x115B8u, 0x115BBu) ||
		iCodepoint == 0x115BEu ||
		__xuiUnicodeRange(iCodepoint, 0x11630u, 0x11632u) ||
		__xuiUnicodeRange(iCodepoint, 0x1163Bu, 0x1163Cu) ||
		iCodepoint == 0x1163Eu ||
		iCodepoint == 0x116ACu ||
		__xuiUnicodeRange(iCodepoint, 0x116AEu, 0x116AFu) ||
		iCodepoint == 0x116B6u ||
		__xuiUnicodeRange(iCodepoint, 0x11720u, 0x11721u) ||
		iCodepoint == 0x11726u ||
		__xuiUnicodeRange(iCodepoint, 0x1182Cu, 0x1182Eu) ||
		iCodepoint == 0x11838u ||
		__xuiUnicodeRange(iCodepoint, 0x11930u, 0x11935u) ||
		__xuiUnicodeRange(iCodepoint, 0x11937u, 0x11938u) ||
		iCodepoint == 0x1193Du ||
		iCodepoint == 0x11940u ||
		iCodepoint == 0x11942u ||
		__xuiUnicodeRange(iCodepoint, 0x119D1u, 0x119D3u) ||
		__xuiUnicodeRange(iCodepoint, 0x119DCu, 0x119DFu) ||
		iCodepoint == 0x119E4u ||
		iCodepoint == 0x11A39u ||
		iCodepoint == 0x11A57u ||
		iCodepoint == 0x11A58u ||
		iCodepoint == 0x11A97u ||
		__xuiUnicodeRange(iCodepoint, 0x11C2Fu, 0x11C2Fu) ||
		iCodepoint == 0x11C3Eu ||
		iCodepoint == 0x11CA9u ||
		iCodepoint == 0x11CB1u ||
		iCodepoint == 0x11CB4u ||
		__xuiUnicodeRange(iCodepoint, 0x11D8Au, 0x11D8Eu) ||
		__xuiUnicodeRange(iCodepoint, 0x11D93u, 0x11D94u) ||
		iCodepoint == 0x11D96u ||
		__xuiUnicodeRange(iCodepoint, 0x11EF5u, 0x11EF6u);
}

static xui_grapheme_class_t __xuiUnicodeClass(uint32_t iCodepoint)
{
	uint32_t iSyllable;

	if ( iCodepoint == 0x000Du ) return XUI_GRAPHEME_CR;
	if ( iCodepoint == 0x000Au ) return XUI_GRAPHEME_LF;
	if ( (iCodepoint <= 0x001Fu) || __xuiUnicodeRange(iCodepoint, 0x007Fu, 0x009Fu) ||
	     iCodepoint == 0x2028u || iCodepoint == 0x2029u ) return XUI_GRAPHEME_CONTROL;
	if ( iCodepoint == 0x200Du ) return XUI_GRAPHEME_ZWJ;
	if ( __xuiUnicodeIsExtend(iCodepoint) || iCodepoint == 0x200Cu ) return XUI_GRAPHEME_EXTEND;
	if ( __xuiUnicodeIsSpacingMark(iCodepoint) ) return XUI_GRAPHEME_SPACING_MARK;
	if ( __xuiUnicodeRange(iCodepoint, 0x1F1E6u, 0x1F1FFu) ) return XUI_GRAPHEME_REGIONAL;
	if ( __xuiUnicodeRange(iCodepoint, 0x0600u, 0x0605u) || iCodepoint == 0x06DDu ||
	     iCodepoint == 0x070Fu || iCodepoint == 0x08E2u ||
	     iCodepoint == 0x110BDu || iCodepoint == 0x110CDu ) return XUI_GRAPHEME_PREPEND;
	if ( __xuiUnicodeRange(iCodepoint, 0x1100u, 0x115Fu) ||
	     __xuiUnicodeRange(iCodepoint, 0xA960u, 0xA97Cu) ) return XUI_GRAPHEME_HANGUL_L;
	if ( __xuiUnicodeRange(iCodepoint, 0x1160u, 0x11A7u) ||
	     __xuiUnicodeRange(iCodepoint, 0xD7B0u, 0xD7C6u) ) return XUI_GRAPHEME_HANGUL_V;
	if ( __xuiUnicodeRange(iCodepoint, 0x11A8u, 0x11FFu) ||
	     __xuiUnicodeRange(iCodepoint, 0xD7CBu, 0xD7FBu) ) return XUI_GRAPHEME_HANGUL_T;
	if ( __xuiUnicodeRange(iCodepoint, 0xAC00u, 0xD7A3u) ) {
		iSyllable = iCodepoint - 0xAC00u;
		return ((iSyllable % 28u) == 0u) ? XUI_GRAPHEME_HANGUL_LV : XUI_GRAPHEME_HANGUL_LVT;
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
