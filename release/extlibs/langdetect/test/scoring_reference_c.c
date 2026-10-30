#define XRT_MODULE_MEMORY_DEBUG
#include "../langdetect_native.c"
#include <stdio.h>

// 只用于对照：逐字节枚举每个词的出现位置，和产品词段遍历保持独立。
static int reference(const char* text, size_t size, const char* const* words, size_t count)
{
    int score = 0;
    for (size_t w = 0; w < count; ++w) {
        size_t length = strlen(words[w]);
        if (!length || length > size) continue;
        for (size_t i = 0; i <= size-length; ++i) {
            size_t j = 0;
            while (j < length && xlangdetect_ascii_lower((uint8)text[i+j]) == (uint8)words[w][j]) ++j;
            if (j == length && !xlangdetect_letter_before(text, size, i) &&
                !xlangdetect_letter_at(text, size, i+length)) ++score;
        }
    }
    return score;
}

int main(void)
{
    static const char* words[] = {
        "the", "and", "is", "of", "to", "in", "that", "with", "this", "for",
        "le", "la", "les", "des", "est", "et", "une", "dans", "pour", "avec",
        "der", "die", "das", "und", "ist", "ein", "eine", "nicht", "mit", "von",
        "el", "la", "los", "las", "es", "en", "que", "de", "para", "con",
        "os", "as", "um", "uma", "para", "com", "dos", "das", "nao",
        "il", "la", "gli", "che", "di", "del", "della", "una", "per", "con",
        "de", "het", "een", "van", "en", "is", "dat", "niet", "voor", "met"
    };
    static const struct { const char* data; size_t size; } edges[] = {
        {"",0}, {"x",1}, {"0",1}, {"_",1}, {"-",1}, {" ",1}, {"\0",1},
        {"\xc3\xa9",2}, {"\xc3\x97",2}, {"\xc3\xb7",2},
        {"\xce\xb1",2}, {"\xcc\x81",2}, {"\xe4\xb8\xad",3},
        {"\xd0\x9f",2}, {"\xd9\x85",2}, {"\xe3\x81\x82",3},
        {"\xed\x95\x9c",3}, {"\xf0\x9f\x98\x80",4}
    };
    unsigned checks = 0, failures = 0;
    char text[128];
    for (size_t w = 0; w < sizeof(words)/sizeof(*words); ++w) {
        for (size_t left = 0; left < sizeof(edges)/sizeof(*edges); ++left) {
            for (size_t right = 0; right < sizeof(edges)/sizeof(*edges); ++right) {
                size_t length = strlen(words[w]);
                size_t used = edges[left].size+length+edges[right].size;
                memcpy(text, edges[left].data, edges[left].size);
                for (size_t j = 0; j < length; ++j)
                    text[edges[left].size+j] = (char)(j%2 ? words[w][j]-'a'+'A' : words[w][j]);
                memcpy(text+edges[left].size+length, edges[right].data, edges[right].size);
                // 单词与重复词表均比较，覆盖共享常见词计入多个模型的行为。
                for (unsigned mode = 0; mode < 2; ++mode) {
                    const char* const* selected = mode ? words : &words[w];
                    size_t count = mode ? sizeof(words)/sizeof(*words) : 1;
                    int expected = reference(text, used, selected, count);
                    if (xlangdetect_score(text, used, selected, count) != expected) ++failures;
                    ++checks;
                }
            }
        }
    }
    // 正常评分不应分配；空词、长词段和多个独立词也纳入同一参照。
    const char* extra[] = {"", "the", "is"};
    memset(text, 't', sizeof(text));
    memcpy(text, "the is the\0is_", 14);
    if (!xrtMemDebugFailAfter(0)) return 2;
    int actual = xlangdetect_score(text, sizeof(text), extra, 3);
    bool injected = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    if (actual != reference(text, sizeof(text), extra, 3) || injected || xrtGetError()) ++failures;
    ++checks;
    printf("langdetect score reference: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
