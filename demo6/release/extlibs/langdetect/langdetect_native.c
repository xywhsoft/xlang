/*
	轻量语言检测。
	先用 XRT 严格解码 UTF-8 统计 Unicode 脚本，再对 Latin 文本做有限词表评分。
	它不是统计语言模型；无证据时返回 unknown，不把 Latin 文本猜成英语。
*/
#include <string.h>
#include "include/langdetect_native.h"

#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "xrt.h"

#define XLANGDETECT_DOMAIN "xlang.extlib.langdetect"
#define XLANGDETECT_MAX_TEXT (1024u * 1024u)

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
static bool xlangdetect_collect_stats(cstr sText, size_t iSize, xlangdetect_stats* pStats)
{
    xstrview Text;
    size_t iOffset = 0;

    if ((sText == NULL) || (pStats == NULL)) {
        xrtSetErrorInfo(XERR_ARGUMENT, XLANGDETECT_DOMAIN, 0,
            "text and output statistics are required");
        return false;
    }
    if (iSize > XLANGDETECT_MAX_TEXT) {
        xrtSetErrorInfo(XERR_RANGE, XLANGDETECT_DOMAIN, 0,
            "text exceeds 1 MiB analysis limit");
        return false;
    }
    memset(pStats, 0, sizeof(*pStats));
    Text = (xstrview){ sText, iSize };
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



static bool xlangdetect_letter_at(cstr sText, size_t iSize, size_t iOffset)
{
    xlangdetect_stats Stats = {0};
    uint32 iScalar;
    size_t iRead = 0u;

    if (iOffset >= iSize) { return false; }
    if ((uint8)sText[iOffset] < 0x80u) {
        return xlangdetect_ascii_letter((uint8)sText[iOffset]);
    }
    if (xrtUtf8Decode((xstrview){sText + iOffset, iSize - iOffset},
        &iScalar, &iRead) != XUTF_OK) { return false; }
    xlangdetect_count_scalar(&Stats, iScalar);
    return Stats.Letters != 0u;
}



static bool xlangdetect_letter_before(cstr sText, size_t iSize, size_t iOffset)
{
    if (iOffset == 0u) { return false; }
    --iOffset;
    while (iOffset > 0u && ((uint8)sText[iOffset] & 0xC0u) == 0x80u) {
        --iOffset;
    }
    return xlangdetect_letter_at(sText, iSize, iOffset);
}



/* 统计 ASCII 词单串的独立出现次数，忽略 ASCII 大小写。 */
static int xlangdetect_word_count(cstr sText, size_t iText, cstr sWord)
{
    size_t iWord = strlen(sWord);
    int iCount = 0;

    if (iWord == 0u || iText < iWord) { return 0; }
    for (size_t i = 0; i + iWord <= iText; i++) {
        size_t j;

        for (j = 0u; j < iWord; j++) {
            if (xlangdetect_ascii_lower((uint8)sText[i + j]) != (uint8)sWord[j]) { break; }
        }
        if (j != iWord) { continue; }
        if (xlangdetect_letter_before(sText, iText, i) ||
            xlangdetect_letter_at(sText, iText, i + iWord)) { continue; }
        ++iCount;
    }
    return iCount;
}



static int xlangdetect_score(cstr sText, size_t iSize, const cstr* arrWords, size_t iCount)
{
    int iScore = 0;

    for (size_t i = 0u; i < iCount; i++) {
        iScore += xlangdetect_word_count(sText, iSize, arrWords[i]);
    }
    return iScore;
}



static xlangdetect_result xlangdetect_script_result(
    const xlangdetect_stats* pStats
)
{
    size_t iKana = pStats->Hiragana + pStats->Katakana;

    /* 少量外文片段不能决定整段文本的语言；日文须有假名证据。 */
    if (iKana >= 2u && (iKana + pStats->Han) * 2u > pStats->Letters) {
        return (xlangdetect_result){ "japanese", "ja", "Japanese", 85 };
    }
    if (pStats->Hangul >= 2u && pStats->Hangul * 2u > pStats->Letters) {
        return (xlangdetect_result){ "korean", "ko", "Korean", 85 };
    }
    if (pStats->Han >= 2u && pStats->Han * 2u > pStats->Letters) {
        return (xlangdetect_result){ "chinese", "zh", "Han", 65 };
    }
    if (pStats->Cyrillic >= 2u && pStats->Cyrillic * 2u > pStats->Letters) {
        return (xlangdetect_result){ "unknown", "und", "Cyrillic", 25 };
    }
    if (pStats->Arabic >= 2u && pStats->Arabic * 2u > pStats->Letters) {
        return (xlangdetect_result){ "unknown", "und", "Arabic", 25 };
    }
    return (xlangdetect_result){ NULL, NULL, NULL, 0 };
}



static xlangdetect_result xlangdetect_latin_result(cstr sText, size_t iSize,
    const xlangdetect_stats* pStats)
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

    /* 词表命中不能让恰好平分的拉丁片段抢走整段文本的语言。 */
    if (pStats->Latin * 2u <= pStats->Letters) {
        return (xlangdetect_result){ "unknown", "und", "Unknown", 0 };
    }
    for (size_t i = 0u; i < sizeof(Models) / sizeof(Models[0]); i++) {
        int iScore = xlangdetect_score(sText, iSize, Models[i].Words, Models[i].WordCount);

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



static xlangdetect_result xlangdetect_analyze(cstr sText, size_t iSize)
{
    xlangdetect_stats Stats;
    xlangdetect_result Result;

    if (!xlangdetect_collect_stats(sText, iSize, &Stats)) {
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
    Result = xlangdetect_latin_result(sText, iSize, &Stats);
    xrtClearError();
    return Result;
}



cstr xlangdetect_detect(cstr sText, size_t iSize)
{
    return xlangdetect_analyze(sText, iSize).Name;
}



cstr xlangdetect_code(cstr sText, size_t iSize)
{
    return xlangdetect_analyze(sText, iSize).Code;
}



cstr xlangdetect_script(cstr sText, size_t iSize)
{
    return xlangdetect_analyze(sText, iSize).Script;
}



int64_t xlangdetect_confidence(cstr sText, size_t iSize)
{
    return (int64_t)xlangdetect_analyze(sText, iSize).Confidence;
}



bool xlangdetect_valid_utf8(cstr sText, size_t iSize)
{
    xrtClearError();
    if (sText == NULL) {
        xrtSetErrorInfo(XERR_ARGUMENT, XLANGDETECT_DOMAIN, 0,
            "text is required");
        return false;
    }
    return xrtUtf8Valid((xstrview){ sText, iSize }, NULL);
}



cstr xlangdetect_last_error(void)
{
    const xerror* pError = xrtGetError();
    cstr sMessage = pError != NULL ? xrtErrorMessage(pError) : NULL;

    return sMessage != NULL ? sMessage : "";
}
