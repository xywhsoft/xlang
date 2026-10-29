/* 首次读取的真实分配失败必须让 C 范例返回失败，不能漏数据后打印 PASS。 */
#define XRT_MODULE_MEMORY_DEBUG
#define main skiplist_example_main
#include "../../../examples/extlibs/skiplist_c.c"
#undef main

int main(void) {
    void *warm = xskiplist_create();
    int result, triggered;
    if (warm == NULL) return 1;
    xskiplist_destroy(warm);
    /* create 两次，三次插入各三次；随后首次读取分配失败。 */
    if (!xrtMemDebugFailAfter(11)) return 1;
    result = skiplist_example_main();
    triggered = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    xrtClearError();
    if (!triggered || result != 1) {
        fprintf(stderr, "skiplist example OOM was swallowed: result=%d triggered=%d\n", result, triggered);
        return 1;
    }
    puts("\nskiplist example OOM: PASS");
    return 0;
}
