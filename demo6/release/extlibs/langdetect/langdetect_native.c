/*
	轻量语言检测。
	先用 XRT 严格解码 UTF-8 统计 Unicode 脚本，再对 Latin 文本做有限词表评分。
	它不是统计语言模型；无证据时返回 unknown，不把 Latin 文本猜成英语。
*/
#include <string.h>

#define XRT_MODULE_UNICODE
#include "xrt.h"

#define XLANGDETECT_DOMAIN "xlang.extlib.langdetect"

typedef enum xlangdetect_script_kind {
    XLANGDETECT_SCRIPT_UNKNOWN = 0,
    XLANGDETECT_SCRIPT_LATIN,
    XLANGDETECT_SCRIPT_HAN,
    XLANGDETECT_SCRIPT_JAPANESE,
    XLANGDETECT_SCRIPT_KOREAN,
    XLANGDETECT_SCRIPT_CYRILLIC,
    XLANGDETECT_SCRIPT_ARABIC
} xlangdetect_script_kind;

typedef struct xlangdetect_stats {
    size_t Scalars;
    size_t Letters;
    size_t Latin;
    size_t Han;
    size_t Hiragana;
    size_t Katakana;
    size_t Hangul;
    size_t Cyrillic;
    size_t Arabic;
} xlangdetect_stats;

typedef struct xlangdetect_result {
    cstr Name;
    cstr Code;
    cstr Script;
    int Confidence;
} xlangdetect_result;



static xstrview xlangdetect_view(cstr sText)
{
    return (xstrview){ sText, sText != NULL ? strlen(sText) : 0u };
}



static bool xlangdetect_ascii_letter(uint8 iByte)
{
    return ((iByte >= 'A') && (iByte <= 'Z')) ||
        ((iByte >= 'a') && (iByte <= 'z'));
}



static uint8 xlangdetect_ascii_lower(uint8 iByte)
{
    return (iByte >= 'A' && iByte <= 'Z') ? (uint8)(iByte + ('a' - 'A')) : iByte;
}



static void xlangdetect_count_scalar(xlangdetect_stats* pStats, uint32 iScalar)
{
    pStats->Scalars++;
    if (((iScalar >= UINT32_C(0x0041)) && (iScalar <= UINT32_C(0x005A))) ||
        ((iScalar >= UINT32_C(0x0061)) && (iScalar <= UINT32_C(0x007A))) ||
        ((iScalar >= UINT32_C(0x00C0)) && (iScalar <= UINT32_C(0x024F)))) {
        pStats->Latin++;
        pStats->Letters++;
    } else if (((iScalar >= UINT32_C(0x3400)) && (iScalar <= UINT32_C(0x4DBF))) ||
        ((iScalar >= UINT32_C(0x4E00)) && (iScalar <= UINT32_C(0x9FFF))) ||
        ((iScalar >= UINT32_C(0xF900)) && (iScalar <= UINT32_C(0xFAFF)))) {
        pStats->Han++;
        pStats->Letters++;
    } else if ((iScalar >= UINT32_C(0x3040)) && (iScalar <= UINT32_C(0x309F))) {
        pStats->Hiragana++;
        pStats->Letters++;
    } else if ((iScalar >= UINT32_C(0x30A0)) && (iScalar <= UINT32_C(0x30FF))) {
        pStats->Katakana++;
        pStats->Letters++;
    } else if (((iScalar >= UINT32_C(0x1100)) && (iScalar <= UINT32_C(0x11FF))) ||
        ((iScalar >= UINT32_C(0xAC00)) && (iScalar <= UINT32_C(0xD7AF)))) {
        pStats->Hangul++;
        pStats->Letters++;
    } else if ((iScalar >= UINT32_C(0x0400)) && (iScalar <= UINT32_C(0x052F))) {
        pStats->Cyrillic++;
        pStats->Letters++;
    } else if ((iScalar >= UINT32_C(0x0600)) && (iScalar <= UINT32_C(0x06FF))) {
        pStats->Arabic++;
        pStats->Letters++;
    }
}



/* 严格统计 Unicode 脚本，拒绝截断和非法 UTF-8。 */
static bool xlangdetect_collect_stats(cstr sText, xlangdetect_stats* pStats)
{
    xstrview Text;
    size_t iOffset = 0;

    if ((sText == NULL) || (pStats == NULL)) {
        xrtSetErrorInfo(XERR_ARGUMENT, XLANGDETECT_DOMAIN, 0,
            "text and output statistics are required");
        return false;
    }
    memset(pStats, 0, sizeof(*pStats));
    Text = xlangdetect_view(sText);
    while (iOffset < Text.Size) {
        uint32 iScalar;
        size_t iRead = 0;

        if (xrtUtf8Decode((xstrview){ Text.Data + iOffset, Text.Size - iOffset },
            &iScalar, &iRead) != XUTF_OK || iRead == 0u) {
            xrtSetErrorInfo(XERR_VALUE, XLANGDETECT_DOMAIN, 0,
                "text is not valid UTF-8");
            return false;
        }
        xlangdetect_count_scalar(pStats, iScalar);
        iOffset += iRead;
    }
    return true;
}



/* 统计 ASCII 词单串的独立出现次数，忽略 ASCII 大小写。 */
static int xlangdetect_word_count(cstr sText, cstr sWord)
{
    size_t iText = strlen(sText);
    size_t iWord = strlen(sWord);
    int iCount = 0;

    if (iWord == 0u || iText < iWord) { return 0; }
    for (size_t i = 0; i + iWord <= iText; i++) {
        size_t j;

        if ((i != 0u) && xlangdetect_ascii_letter((uint8)sText[i - 1u])) { continue; }
        if ((i + iWord < iText) && xlangdetect_ascii_letter((uint8)sText[i + iWord])) { continue; }
        for (j = 0u; j < iWord; j++) {
            if (xlangdetect_ascii_lower((uint8)sText[i + j]) != (uint8)sWord[j]) { break; }
        }
        if (j == iWord) { iCount++; }
    }
    return iCount;
}



static int xlangdetect_score(cstr sText, const cstr* arrWords, size_t iCount)
{
    int iScore = 0;

    for (size_t i = 0u; i < iCount; i++) {
        iScore += xlangdetect_word_count(sText, arrWords[i]);
    }
    return iScore;
}



static xlangdetect_result xlangdetect_script_result(
    const xlangdetect_stats* pStats
)
{
    if (pStats->Hiragana + pStats->Katakana != 0u) {
        return (xlangdetect_result){ "japanese", "ja", "Japanese", 99 };
    }
    if (pStats->Hangul != 0u) {
        return (xlangdetect_result){ "korean", "ko", "Korean", 99 };
    }
    if (pStats->Han != 0u) {
        return (xlangdetect_result){ "chinese", "zh", "Han", 98 };
    }
    if (pStats->Cyrillic != 0u) {
        return (xlangdetect_result){ "russian", "ru", "Cyrillic", 96 };
    }
    if (pStats->Arabic != 0u) {
        return (xlangdetect_result){ "arabic", "ar", "Arabic", 96 };
    }
    return (xlangdetect_result){ NULL, NULL, NULL, 0 };
}



static xlangdetect_result xlangdetect_latin_result(cstr sText, const xlangdetect_stats* pStats)
{
    static const cstr English[] = { "the", "and", "is", "of", "to", "in", "that", "with", "this", "for" };
    static const cstr French[] = { "le", "la", "les", "des", "est", "et", "une", "dans", "pour", "avec" };
    static const cstr German[] = { "der", "die", "das", "und", "ist", "ein", "eine", "nicht", "mit", "von" };
    static const cstr Spanish[] = { "el", "la", "los", "las", "es", "en", "que", "de", "para", "con" };
    static const cstr Portuguese[] = { "os", "as", "um", "uma", "para", "com", "dos", "das", "nao" };
    static const cstr Italian[] = { "il", "la", "gli", "che", "di", "del", "della", "una", "per", "con" };
    static const cstr Dutch[] = { "de", "het", "een", "van", "en", "is", "dat", "niet", "voor", "met" };
    static const struct {
        cstr Name;
        cstr Code;
        const cstr* Words;
        size_t WordCount;
    } Models[] = {
        { "english", "en", English, sizeof(English) / sizeof(English[0]) },
        { "french", "fr", French, sizeof(French) / sizeof(French[0]) },
        { "german", "de", German, sizeof(German) / sizeof(German[0]) },
        { "spanish", "es", Spanish, sizeof(Spanish) / sizeof(Spanish[0]) },
        { "portuguese", "pt", Portuguese, sizeof(Portuguese) / sizeof(Portuguese[0]) },
        { "italian", "it", Italian, sizeof(Italian) / sizeof(Italian[0]) },
        { "dutch", "nl", Dutch, sizeof(Dutch) / sizeof(Dutch[0]) }
    };
    int iBest = 0;
    int iSecond = 0;
    size_t iIndex = 0u;

    if (pStats->Latin == 0u) {
        return (xlangdetect_result){ "unknown", "und", "Unknown", 0 };
    }
    for (size_t i = 0u; i < sizeof(Models) / sizeof(Models[0]); i++) {
        int iScore = xlangdetect_score(sText, Models[i].Words, Models[i].WordCount);

        if (iScore > iBest) {
            iSecond = iBest;
            iBest = iScore;
            iIndex = i;
        } else if (iScore > iSecond) {
            iSecond = iScore;
        }
    }
    if (iBest == 0 || iBest == iSecond) {
        return (xlangdetect_result){ "unknown", "und", "Latin", 15 };
    }
    {
        int iConfidence = 45 + iBest * 7 + (iBest - iSecond) * 5;
        if (iConfidence > 95) { iConfidence = 95; }
        return (xlangdetect_result){ Models[iIndex].Name, Models[iIndex].Code, "Latin", iConfidence };
    }
}



static xlangdetect_result xlangdetect_analyze(cstr sText)
{
    xlangdetect_stats Stats;
    xlangdetect_result Result;

    if (!xlangdetect_collect_stats(sText, &Stats)) {
        return (xlangdetect_result){ "unknown", "und", "Unknown", 0 };
    }
    if (Stats.Letters == 0u) {
        xrtClearError();
        return (xlangdetect_result){ "unknown", "und", "Unknown", 0 };
    }
    Result = xlangdetect_script_result(&Stats);
    if (Result.Name != NULL) {
        xrtClearError();
        return Result;
    }
    Result = xlangdetect_latin_result(sText, &Stats);
    xrtClearError();
    return Result;
}



cstr xlangdetect_detect(cstr sText)
{
    return xlangdetect_analyze(sText).Name;
}



cstr xlangdetect_code(cstr sText)
{
    return xlangdetect_analyze(sText).Code;
}



cstr xlangdetect_script(cstr sText)
{
    return xlangdetect_analyze(sText).Script;
}



long long xlangdetect_confidence(cstr sText)
{
    return (long long)xlangdetect_analyze(sText).Confidence;
}



bool xlangdetect_valid_utf8(cstr sText)
{
    if (sText == NULL) { return false; }
    return xrtUtf8Valid(xlangdetect_view(sText), NULL);
}



cstr xlangdetect_last_error(void)
{
    const xerror* pError = xrtGetError();
    cstr sMessage = pError != NULL ? xrtErrorMessage(pError) : NULL;

    return sMessage != NULL ? sMessage : "";
}
