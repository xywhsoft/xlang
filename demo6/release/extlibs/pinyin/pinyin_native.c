/*
 * pinyin_native.c - Chinese character to pinyin conversion (common subset)
 * Uses UTF-8 encoded lookup table for ~200 most common characters.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern void xrtSetError(const char *msg);

static char py_buf[8192];

typedef struct { unsigned int code; const char *py; } PyEntry;

/* UTF-8 codepoint -> pinyin for common characters */
static const PyEntry py_table[] = {
    {0x4E00,"yi"},{0x4E01,"ding"},{0x4E03,"qi"},{0x4E07,"wan"},{0x4E08,"zhang"},
    {0x4E09,"san"},{0x4E0A,"shang"},{0x4E0B,"xia"},{0x4E0D,"bu"},{0x4E0E,"yu"},
    {0x4E10,"gai"},{0x4E11,"chou"},{0x4E13,"zhuan"},{0x4E14,"qie"},{0x4E15,"pi"},
    {0x4E16,"shi"},{0x4E18,"qiu"},{0x4E19,"bing"},{0x4E1A,"ye"},{0x4E1B,"cong"},
    {0x4E1C,"dong"},{0x4E22,"diu"},{0x4E24,"liang"},{0x4E25,"yan"},{0x4E27,"sang"},
    {0x4E2A,"ge"},{0x4E2D,"zhong"},{0x4E30,"feng"},{0x4E32,"chuan"},{0x4E34,"lin"},
    {0x4E3A,"wei"},{0x4E3B,"zhu"},{0x4E43,"nai"},{0x4E45,"jiu"},{0x4E48,"me"},
    {0x4E49,"yi"},{0x4E4B,"zhi"},{0x4E4E,"hu"},{0x4E4F,"fa"},{0x4E50,"le"},
    {0x4E52,"ping"},{0x4E53,"pang"},{0x4E58,"cheng"},{0x4E59,"yi"},{0x4E5C,"mie"},
    {0x4E5D,"jiu"},{0x4E5E,"qi"},{0x4E5F,"ye"},{0x4E60,"xi"},{0x4E61,"xiang"},
    {0x4E66,"shu"},{0x4E70,"mai"},{0x4E71,"luan"},{0x4E73,"ru"},{0x4E7E,"qian"},
    {0x4E86,"le"},{0x4E88,"yu"},{0x4E89,"zheng"},{0x4E8B,"shi"},{0x4E8C,"er"},
    {0x4E8E,"yu"},{0x4E91,"yun"},{0x4E92,"hu"},{0x4E94,"wu"},{0x4E95,"jing"},
    {0x4E9A,"ya"},{0x4E9B,"xie"},{0x4EA1,"wang"},{0x4EA4,"jiao"},{0x4EA6,"yi"},
    {0x4EA7,"chan"},{0x4EAB,"xiang"},{0x4EAC,"jing"},{0x4EAD,"ting"},{0x4EBA,"ren"},
    {0x4EC0,"shen"},{0x4EC1,"ren"},{0x4EC5,"jin"},{0x4EC6,"pu"},{0x4EC7,"chou"},
    {0x4ECA,"jin"},{0x4ECB,"jie"},{0x4ECD,"reng"},{0x4ECE,"cong"},{0x4ED6,"ta"},
    {0x4ED8,"fu"},{0x4EE3,"dai"},{0x4EE4,"ling"},{0x4EE5,"yi"},{0x4EEC,"men"},
    {0x4EF6,"jian"},{0x4EFB,"ren"},{0x4EFD,"fen"},{0x4EFF,"fang"},{0x4F01,"qi"},
    {0x4F09,"jiu"},{0x4F0A,"yi"},{0x4F11,"xiu"},{0x4F17,"zhong"},{0x4F18,"you"},
    {0x4F19,"huo"},{0x4F1A,"hui"},{0x4F20,"chuan"},{0x4F24,"shang"},{0x4F25,"chang"},
    {0x4F26,"lun"},{0x4F46,"dan"},{0x4F4D,"wei"},{0x4F4E,"di"},{0x4F53,"ti"},
    {0x4F55,"he"},{0x4F59,"yu"},{0x4F5B,"fo"},{0x4F5C,"zuo"},{0x4F60,"ni"},
    {0x4F7F,"shi"},{0x4F8B,"li"},{0x4F9B,"gong"},{0x4FDD,"bao"},{0x4FE1,"xin"},
    {0x5019,"hou"},{0x501F,"jie"},{0x503C,"zhi"},{0x505A,"zuo"},{0x5065,"jian"},
    {0x5143,"yuan"},{0x5148,"xian"},{0x5149,"guang"},{0x514B,"ke"},{0x515A,"dang"},
    {0x5165,"ru"},{0x5168,"quan"},{0x516C,"gong"},{0x5171,"gong"},{0x5173,"guan"},
    {0x5174,"xing"},{0x5176,"qi"},{0x5177,"ju"},{0x5185,"nei"},{0x518D,"zai"},
    {0x5199,"xie"},{0x51FA,"chu"},{0x5206,"fen"},{0x5207,"qie"},{0x521B,"chuang"},
    {0x522B,"bie"},{0x5230,"dao"},{0x5236,"zhi"},{0x524D,"qian"},{0x529B,"li"},
    {0x529E,"ban"},{0x52A0,"jia"},{0x52A8,"dong"},{0x52A9,"zhu"},{0x52B1,"li"},
    {0x5316,"hua"},{0x5317,"bei"},{0x533A,"qu"},{0x533B,"yi"},{0x5341,"shi"},
    {0x5343,"qian"},{0x5347,"sheng"},{0x534A,"ban"},{0x5355,"dan"},{0x5357,"nan"},
    {0x535A,"bo"},{0x539F,"yuan"},{0x53BB,"qu"},{0x53C8,"you"},{0x53CA,"ji"},
    {0x53CB,"you"},{0x53CC,"shuang"},{0x53D1,"fa"},{0x53D6,"qu"},{0x53D7,"shou"},
    {0x53E3,"kou"},{0x53EA,"zhi"},{0x53EF,"ke"},{0x53F0,"tai"},{0x53F2,"shi"},
    {0x53F7,"hao"},{0x5404,"ge"},{0x5408,"he"},{0x540C,"tong"},{0x540D,"ming"},
    {0x540E,"hou"},{0x5411,"xiang"},{0x542C,"ting"},{0x544A,"gao"},{0x5458,"yuan"},
    {0x5462,"ne"},{0x548C,"he"},{0x54C1,"pin"},{0x5668,"qi"},{0x56DB,"si"},
    {0x56DE,"hui"},{0x56E0,"yin"},{0x56FD,"guo"},{0x5728,"zai"},{0x5730,"di"},
    {0x573A,"chang"},{0x57CE,"cheng"},{0x58EB,"shi"},{0x5904,"chu"},{0x5907,"bei"},
    {0x5916,"wai"},{0x591A,"duo"},{0x5927,"da"},{0x5929,"tian"},{0x592A,"tai"},
    {0x5934,"tou"},{0x5973,"nv"},{0x597D,"hao"},{0x5982,"ru"},{0x59CB,"shi"},
    {0x5B50,"zi"},{0x5B57,"zi"},{0x5B66,"xue"},{0x5B83,"ta"},{0x5B89,"an"},
    {0x5B8C,"wan"},{0x5B9A,"ding"},{0x5B9E,"shi"},{0x5BB6,"jia"},{0x5BF9,"dui"},
    {0x5C06,"jiang"},{0x5C0F,"xiao"},{0x5C11,"shao"},{0x5C31,"jiu"},{0x5C71,"shan"},
    {0x5DE5,"gong"},{0x5DF1,"ji"},{0x5DF2,"yi"},{0x5E02,"shi"},{0x5E73,"ping"},
    {0x5E74,"nian"},{0x5E76,"bing"},{0x5E94,"ying"},{0x5E95,"di"},{0x5EA6,"du"},
    {0x5EFA,"jian"},{0x5F00,"kai"},{0x5F0F,"shi"},{0x5F15,"yin"},{0x5F53,"dang"},
    {0x5F55,"lu"},{0x5F62,"xing"},{0x5F88,"hen"},{0x5F97,"de"},{0x5FC3,"xin"},
    {0x5FC5,"bi"},{0x5FEB,"kuai"},{0x601D,"si"},{0x6027,"xing"},{0x60C5,"qing"},
    {0x6210,"cheng"},{0x6211,"wo"},{0x6216,"huo"},{0x6240,"suo"},{0x624B,"shou"},
    {0x624D,"cai"},{0x6253,"da"},{0x628A,"ba"},{0x62A5,"bao"},{0x62C9,"la"},
    {0x62CD,"pai"},{0x6307,"zhi"},{0x636E,"ju"},{0x63A5,"jie"},{0x63D0,"ti"},
    {0x6536,"shou"},{0x6539,"gai"},{0x653E,"fang"},{0x653F,"zheng"},{0x6559,"jiao"},
    {0x6570,"shu"},{0x6587,"wen"},{0x65B0,"xin"},{0x65B9,"fang"},{0x65E0,"wu"},
    {0x65E5,"ri"},{0x65F6,"shi"},{0x660E,"ming"},{0x662F,"shi"},{0x66F4,"geng"},
    {0x6700,"zui"},{0x6708,"yue"},{0x6709,"you"},{0x672C,"ben"},{0x673A,"ji"},
    {0x6743,"quan"},{0x6765,"lai"},{0x679C,"guo"},{0x6B21,"ci"},{0x6B63,"zheng"},
    {0x6B64,"ci"},{0x6B7B,"si"},{0x6C11,"min"},{0x6C14,"qi"},{0x6C34,"shui"},
    {0x6CA1,"mei"},{0x6CD5,"fa"},{0x6CE8,"zhu"},{0x6D3B,"huo"},{0x6D41,"liu"},
    {0x6E38,"you"},{0x706B,"huo"},{0x70B9,"dian"},{0x7231,"ai"},{0x7247,"pian"},
    {0x7248,"ban"},{0x7269,"wu"},{0x7279,"te"},{0x738B,"wang"},{0x73B0,"xian"},
    {0x7406,"li"},{0x751F,"sheng"},{0x7528,"yong"},{0x7531,"you"},{0x7535,"dian"},
    {0x767D,"bai"},{0x7684,"de"},{0x76EE,"mu"},{0x76F4,"zhi"},{0x76F8,"xiang"},
    {0x770B,"kan"},{0x771F,"zhen"},{0x77E5,"zhi"},{0x7801,"ma"},{0x786E,"que"},
    {0x793E,"she"},{0x795E,"shen"},{0x79D1,"ke"},{0x7A0B,"cheng"},{0x7B2C,"di"},
    {0x7B49,"deng"},{0x7CFB,"xi"},{0x7EA7,"ji"},{0x7EC4,"zu"},{0x7ED3,"jie"},
    {0x7ED9,"gei"},{0x7F51,"wang"},{0x8005,"zhe"},{0x800C,"er"},{0x80FD,"neng"},
    {0x81EA,"zi"},{0x884C,"xing"},{0x8868,"biao"},{0x8981,"yao"},{0x89C1,"jian"},
    {0x89C4,"gui"},{0x89E3,"jie"},{0x8BA4,"ren"},{0x8BA9,"rang"},{0x8BB0,"ji"},
    {0x8BB8,"xu"},{0x8BBA,"lun"},{0x8BBE,"she"},{0x8BC1,"zheng"},{0x8BC4,"ping"},
    {0x8BD5,"shi"},{0x8BDD,"hua"},{0x8BE5,"gai"},{0x8BF4,"shuo"},{0x8BF7,"qing"},
    {0x8BFB,"du"},{0x8C01,"shei"},{0x8C03,"diao"},{0x8D77,"qi"},{0x8DEF,"lu"},
    {0x8F66,"che"},{0x8FDB,"jin"},{0x8FD8,"hai"},{0x8FD9,"zhe"},{0x8FDE,"lian"},
    {0x9009,"xuan"},{0x90A3,"na"},{0x90FD,"dou"},{0x91CC,"li"},{0x91CD,"zhong"},
    {0x91CF,"liang"},{0x94B1,"qian"},{0x9519,"cuo"},{0x95E8,"men"},{0x95EE,"wen"},
    {0x95F4,"jian"},{0x9645,"ji"},{0x9650,"xian"},{0x9662,"yuan"},{0x9762,"mian"},
    {0x97F3,"yin"},{0x9879,"xiang"},{0x9898,"ti"},{0x98CE,"feng"},{0x9AD8,"gao"},
    {0, NULL}
};

static const char *py_lookup(unsigned int codepoint) {
    int i;
    for (i = 0; py_table[i].py; i++) {
        if (py_table[i].code == codepoint) return py_table[i].py;
    }
    return NULL;
}

/* Convert UTF-8 text to space-separated pinyin */
const char *xpinyin_convert(const char *text) {
    int pos = 0;
    const unsigned char *p = (const unsigned char *)text;
    int first = 1;
    if (!text) return "";
    py_buf[0] = '\0';

    while (*p) {
        unsigned int cp = 0;
        int bytes = 0;
        if (*p < 0x80) { cp = *p; bytes = 1; }
        else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; bytes = 2; }
        else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; bytes = 3; }
        else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; bytes = 4; }
        else { p++; continue; }

        { int i; for (i = 1; i < bytes; i++) { if (!p[i]) break; cp = (cp << 6) | (p[i] & 0x3F); } }
        p += bytes;

        if (cp < 0x80) {
            /* ASCII: pass through */
            if (pos < (int)sizeof(py_buf) - 2) py_buf[pos++] = (char)cp;
        } else {
            const char *py = py_lookup(cp);
            if (py) {
                if (!first && pos < (int)sizeof(py_buf) - 1) py_buf[pos++] = ' ';
                int len = (int)strlen(py);
                if (pos + len < (int)sizeof(py_buf) - 1) { memcpy(py_buf + pos, py, len); pos += len; }
                first = 0;
            }
        }
    }
    py_buf[pos] = '\0';
    return py_buf;
}

/* Get first letters (initials) of pinyin */
const char *xpinyin_initials(const char *text) {
    int pos = 0;
    const unsigned char *p = (const unsigned char *)text;
    if (!text) return "";
    py_buf[0] = '\0';

    while (*p) {
        unsigned int cp = 0;
        int bytes = 0;
        if (*p < 0x80) { cp = *p; bytes = 1; }
        else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; bytes = 2; }
        else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; bytes = 3; }
        else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; bytes = 4; }
        else { p++; continue; }
        { int i; for (i = 1; i < bytes; i++) { if (!p[i]) break; cp = (cp << 6) | (p[i] & 0x3F); } }
        p += bytes;

        if (cp >= 0x80) {
            const char *py = py_lookup(cp);
            if (py && pos < (int)sizeof(py_buf) - 1) py_buf[pos++] = py[0];
        }
    }
    py_buf[pos] = '\0';
    return py_buf;
}
