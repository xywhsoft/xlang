#define XRT_MODULE_ALL
#include "xrt.h"
#include "../md4c_native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 只替换本测试中解析器的 CRT 分配；正式库和 XRT 的分配路径不变。 */
static struct { void* Pointer; size_t Size; } parser_blocks[512];
static size_t parser_budget, parser_calls, parser_live, parser_invalid;
static bool parser_failed, parser_fail_once;

static void* parser_malloc(size_t size)
{
    ++parser_calls;
    if (parser_budget == 0) {
        parser_failed = true;
        if (parser_fail_once) parser_budget = (size_t)-1;
        return NULL;
    }
    --parser_budget;
    for (size_t i = 0; i < sizeof(parser_blocks) / sizeof(parser_blocks[0]); ++i) {
        if (parser_blocks[i].Pointer) continue;
        void* p = malloc(size ? size : 1);
        if (!p) { parser_failed = true; return NULL; }
        parser_blocks[i].Pointer = p;
        parser_blocks[i].Size = size;
        ++parser_live;
        return p;
    }
    fprintf(stderr, "parser allocation tracker exhausted\n");
    exit(2);
}

static void parser_free(void* p)
{
    if (!p) return;
    for (size_t i = 0; i < sizeof(parser_blocks) / sizeof(parser_blocks[0]); ++i) {
        if (parser_blocks[i].Pointer != p) continue;
        parser_blocks[i].Pointer = NULL;
        --parser_live;
        free(p);
        return;
    }
    /* 记录重复释放而不再次访问坏指针，使整轮逐点扫描能报告其余问题。 */
    ++parser_invalid;
}

static void* parser_realloc(void* p, size_t size)
{
    if (!p) return parser_malloc(size);
    for (size_t i = 0; i < sizeof(parser_blocks) / sizeof(parser_blocks[0]); ++i) {
        if (parser_blocks[i].Pointer != p) continue;
        size_t old_size = parser_blocks[i].Size;
        /* 强制搬迁，稳定覆盖 realloc 成功后旧地址立即失效的合法行为。 */
        void* replacement = parser_malloc(size);
        if (replacement) {
            memcpy(replacement, p, old_size < size ? old_size : size);
            parser_free(p);
        }
        return replacement;
    }
    ++parser_invalid;
    return NULL;
}

#define malloc parser_malloc
#define realloc parser_realloc
#define free parser_free
#include "../third_party/md4c/md4c.c"
#undef malloc
#undef realloc
#undef free
#undef ISDIGIT
#undef ISLOWER
#undef ISUPPER
#undef ISALNUM
#include "../third_party/md4c/md4c-html.c"
#include "../third_party/md4c/entity.c"
#include "../md4c_native.c"

int main(void)
{
    static const char* inputs[] = {
        "# Title\n\nplain **bold** &amp; text\n",
        "> quote\n>\n> - one\n>   - two\n>     - three\n",
        "| a | b |\n|:---|---:|\n| **one** | `two` |\n",
        "[x](https://e.test/?a=1&amp;b=2&amp;c=3&amp;d=4&amp;e=5&amp;f=6&amp;g=7&amp;h=8 \"A &amp; B &copy; C\")\n",
        "[a]: /one \"title &amp; more\"\n[b]: /two\n\n[a] [b] [a]\n",
        "[two\nlines]: /target\n\n[two\nlines]\n",
        "```lang&amp;x\ncontent\n```\n",
        "[[page&amp;x|label]]\n",
        "[last]: /end \"title\"\n",
        "> [last]: /end \"title\"\n>\n> text\n",
        "- [last]: /end \"title\"\n\n  paragraph\n",
        "[two\nlines]: /end \"title\"\n",
        "[last]: /end \"two\nlines\"\n- list\n"
    };
    unsigned failures = 0, budgets = 0, injected = 0;
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    /* 持续内存耗尽与只失败一次都要覆盖，后者能发现错误被忽略的路径。 */
    for (unsigned mode = 0; mode < 2; ++mode)
      for (size_t input = 0; input < sizeof(inputs) / sizeof(inputs[0]); ++input) {
        parser_fail_once = mode != 0;
        bool completed = false;
        for (size_t budget = 0; budget < 512; ++budget) {
            parser_budget = budget;
            parser_calls = parser_invalid = 0;
            parser_failed = false;
            char* html = xmd4c_html_flags(inputs[input], xmd4c_github_flags() | MD_FLAG_WIKILINKS, 0);
            bool failed = parser_failed;
            bool ok = failed ? html == NULL && xrtGetError() &&
                xrtErrorKind(xrtGetError()) == XERR_MEMORY : html != NULL && !xrtGetError();
            if (!ok || parser_live || parser_invalid) {
                fprintf(stderr, "parser OOM mode=%u input=%zu budget=%zu calls=%zu injected=%d kind=%d live=%zu invalid=%zu\n",
                    mode, input, budget, parser_calls, (int)failed,
                    xrtGetError() ? (int)xrtErrorKind(xrtGetError()) : 0, parser_live, parser_invalid);
                ++failures;
            }
            xrtFree(html);
            ++budgets;
            if (failed) ++injected;
            /* 保留失败计数，但释放漏出的测试分配，避免污染下一次扫描。 */
            for (size_t i = 0; i < sizeof(parser_blocks) / sizeof(parser_blocks[0]); ++i) {
                if (parser_blocks[i].Pointer) parser_free(parser_blocks[i].Pointer);
            }
            if (!failed) { completed = true; break; }
        }
        if (!completed) ++failures;
        parser_budget = (size_t)-1;
        char* retry = xmd4c_html("after failure");
        if (!retry || strcmp(retry, "<p>after failure</p>\n") || xrtGetError() || parser_live || parser_invalid) ++failures;
        xrtFree(retry);
    }
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    if (!injected || before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes ||
        before.InvalidFreeCount != after.InvalidFreeCount || before.DoubleFreeCount != after.DoubleFreeCount ||
        before.OverflowCount != after.OverflowCount || before.UnderflowCount != after.UnderflowCount) ++failures;
    printf("md4c parser CRT allocation: %u budgets, %u injected, %u failures\n", budgets, injected, failures);
    return failures ? 1 : 0;
}
