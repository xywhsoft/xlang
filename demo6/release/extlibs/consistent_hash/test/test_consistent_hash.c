#define XRT_MODULE_ALL
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/consistent_hash_native.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "consistent_hash C test failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (0)

static int test_allocation_failure(void)
{
    unsigned failure;
    unsigned rejected = 0u;
    unsigned succeeded = 0u;
    {
        void* ring;
        CHECK(xrtMemDebugFailAfter(0u));
        CHECK(xchash_create(8) == NULL && xrtMemDebugFailTriggered() &&
            xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        xrtMemDebugFailClear();
        ring = xchash_create(8);
        CHECK(ring != NULL && xrtGetError() == NULL);
        xchash_destroy(ring);
    }
    for (failure = 0u; failure < 32u && succeeded < 2u; ++failure) {
        void* ring = xchash_create(8);
        int64_t added;
        int triggered;
        char* selected;
        CHECK(ring != NULL && xchash_add_node(ring, "node-A") == 1);
        selected = (char*)xchash_get_node(ring, "stable-key");
        CHECK(selected != NULL && strcmp(selected, "node-A") == 0);
        CHECK(xrtMemDebugFailAfter(failure));
        added = xchash_add_node(ring, "node-B");
        triggered = xrtMemDebugFailTriggered();
        CHECK(triggered ? xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_MEMORY : xrtGetError() == NULL);
        xrtMemDebugFailClear();
        xrtClearError();
        if (triggered) {
            char* current = (char*)xchash_get_node(ring, "stable-key");
            CHECK(added == 0 && xchash_node_count(ring) == 1);
            CHECK(xchash_vnode_count(ring) == 8);
            CHECK(current != NULL && strcmp(current, selected) == 0);
            xrtFree(current);
            ++rejected;
        } else {
            CHECK(added == 1 && xchash_node_count(ring) == 2);
            CHECK(xchash_vnode_count(ring) == 16);
            ++succeeded;
        }
        xrtFree(selected);
        xchash_destroy(ring);
    }
    CHECK(rejected >= 2u && succeeded == 2u);
    {
        void* ring = xchash_create(2);
        CHECK(ring != NULL && xchash_add_node(ring, "node-A") == 1);
        CHECK(xrtMemDebugFailAfter(0u));
        CHECK(xchash_get_node(ring, "key") == NULL);
        CHECK(xrtMemDebugFailTriggered() && xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        xrtMemDebugFailClear();
        CHECK(xchash_node_count(ring) == 1);
        {
            char* selected = (char*)xchash_get_node(ring, "key");
            CHECK(selected != NULL && strcmp(selected, "node-A") == 0);
            xrtFree(selected);
        }
        xchash_destroy(ring);
    }
    return 0;
}

static int test_dual_growth_failure(void)
{
    static const char* names[] = {"node-A", "node-B", "node-C", "node-D"};
    unsigned failure, rejected = 0u, succeeded = 0u;
    for (failure = 0u; failure < 8u; ++failure) {
        void* ring = xchash_create(8);
        char* selected;
        int64_t added;
        int triggered;
        size_t i;
        CHECK(ring != NULL);
        for (i = 0u; i < sizeof(names) / sizeof(names[0]); ++i)
            CHECK(xchash_add_node(ring, names[i]) == 1);
        CHECK(xchash_node_count(ring) == 4 && xchash_vnode_count(ring) == 32);
        selected = (char*)xchash_get_node(ring, "stable-key");
        CHECK(selected != NULL);
        CHECK(xrtMemDebugFailAfter(failure));
        added = xchash_add_node(ring, "node-E");
        triggered = xrtMemDebugFailTriggered();
        CHECK(triggered ? xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_MEMORY : xrtGetError() == NULL);
        xrtMemDebugFailClear();
        if (triggered) {
            char* current = (char*)xchash_get_node(ring, "stable-key");
            CHECK(added == 0 && xchash_node_count(ring) == 4 &&
                xchash_vnode_count(ring) == 32 && current != NULL &&
                strcmp(current, selected) == 0);
            xrtFree(current);
            CHECK(xchash_add_node(ring, "node-E") == 1);
            ++rejected;
        } else {
            CHECK(added == 1);
            ++succeeded;
        }
        CHECK(xchash_node_count(ring) == 5 && xchash_vnode_count(ring) == 40);
        xrtFree(selected);
        xchash_destroy(ring);
    }
    CHECK(rejected >= 3u && succeeded > 0u);
    return 0;
}

static int test_name_limit(void)
{
    void* ring = xchash_create(10000);
    char name[4098];
    char* selected;
    CHECK(ring != NULL);
    memset(name, 'n', 4096u);
    name[4096] = '\0';
    CHECK(xchash_add_node(ring, name));
    CHECK(xchash_node_count(ring) == 1 && xchash_vnode_count(ring) == 10000);
    selected = (char*)xchash_get_node(ring, "boundary-key");
    CHECK(selected != NULL && strcmp(selected, name) == 0);
    xrtFree(selected);
    name[4096] = 'n';
    name[4097] = '\0';
    CHECK(!xchash_add_node(ring, name));
    CHECK(xchash_node_count(ring) == 1 && xchash_vnode_count(ring) == 10000);
    name[4096] = '\0';
    CHECK(xchash_remove_node(ring, name));
    xchash_destroy(ring);
    return 0;
}

static int test_fixed_layout(void)
{
    static const char* keys[] = {
        "", "a", "b", "c", "request-0", "request-1", "request-2",
        "request-10", "request-99", "stable-key",
        "\xe4\xbd\xa0\xe5\xa5\xbd", "key:with/slash"
    };
    static const char* expected[] = {
        "node-B", "node-B", "node-B", "node-A", "node-A", "node-B",
        "node-C", "node-B", "node-B", "node-A", "node-C", "node-C"
    };
    void* ring = xchash_create(64);
    size_t i;
    CHECK(ring != NULL);
    CHECK(xchash_add_node(ring, "node-A") == 1);
    CHECK(xchash_add_node(ring, "node-B") == 1);
    CHECK(xchash_add_node(ring, "node-C") == 1);
    for (i = 0u; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        char* selected = (char*)xchash_get_node(ring, keys[i]);
        CHECK(selected != NULL && strcmp(selected, expected[i]) == 0 &&
            xrtGetError() == NULL);
        xrtFree(selected);
    }
    xchash_destroy(ring);
    return 0;
}

static int test_error_recovery(void)
{
    void* ring;
    char* selected;
    xrtClearError();
    CHECK(xchash_create(0) == NULL && xrtGetError());
    ring = xchash_create(2);
    CHECK(ring != NULL && xrtGetError() == NULL);

    CHECK(xchash_add_node(ring, "") == 0 && xrtGetError());
    CHECK(xchash_add_node(ring, "node-A") == 1 && xrtGetError() == NULL);
    CHECK(xchash_remove_node(ring, NULL) == 0 && xrtGetError());
    CHECK(xchash_remove_node(ring, "missing") == 1 && xrtGetError() == NULL);
    CHECK(xchash_get_node(ring, NULL) == NULL && xrtGetError());
    selected = (char*)xchash_get_node(ring, "key");
    CHECK(selected && strcmp(selected, "node-A") == 0 && xrtGetError() == NULL);
    xrtFree(selected);

    CHECK(xchash_node_count(NULL) == -1 && xrtGetError());
    CHECK(xchash_node_count(ring) == 1 && xrtGetError() == NULL);
    CHECK(xchash_vnode_count(NULL) == -1 && xrtGetError());
    CHECK(xchash_vnode_count(ring) == 2 && xrtGetError() == NULL);
    xchash_destroy(ring);
    return 0;
}

int main(void)
{
    void* forward = xchash_create(64);
    void* reverse = xchash_create(64);
    char key[48];
    char long_node[1100];
    char* saved;
    int i;

    CHECK(xchash_create(0) == NULL);
    CHECK(xchash_create(10001) == NULL);
    CHECK(forward != NULL && reverse != NULL);
    CHECK(xchash_add_node(forward, "node-A"));
    CHECK(xchash_add_node(forward, "node-B"));
    CHECK(xchash_add_node(forward, "node-C"));
    CHECK(xchash_add_node(reverse, "node-C"));
    CHECK(xchash_add_node(reverse, "node-B"));
    CHECK(xchash_add_node(reverse, "node-A"));
    CHECK(xchash_add_node(forward, "node-A"));
    CHECK(xchash_node_count(forward) == 3 && xchash_vnode_count(forward) == 192);

    for (i = 0; i < 1000; ++i) {
        char* a;
        char* b;
        snprintf(key, sizeof(key), "request-%d", i);
        a = (char*)xchash_get_node(forward, key);
        b = (char*)xchash_get_node(reverse, key);
        CHECK(a != NULL && b != NULL && strcmp(a, b) == 0);
        xrtFree(a);
        xrtFree(b);
    }
    saved = (char*)xchash_get_node(forward, "persistent-key");
    CHECK(saved != NULL);
    for (i = 0; i < 128; ++i) {
        char* result = (char*)xchash_get_node(forward, "another-key");
        CHECK(result != NULL && strlen(saved) == 6u);
        xrtFree(result);
    }
    xrtFree(saved);

    /* 两个长名称前缀相同；虚拟节点哈希不得只看前 512 字节。 */
    memset(long_node, 'x', sizeof(long_node) - 1u);
    long_node[sizeof(long_node) - 1u] = '\0';
    CHECK(xchash_add_node(forward, long_node));
    CHECK(xchash_node_count(forward) == 4);
    long_node[sizeof(long_node) - 2u] = 'y';
    CHECK(xchash_add_node(forward, long_node));
    CHECK(xchash_node_count(forward) == 5);
    CHECK(!xchash_add_node(forward, ""));
    CHECK(!xchash_add_node(forward, "\xc3\x28"));

    CHECK(xchash_remove_node(forward, long_node));
    long_node[sizeof(long_node) - 2u] = 'x';
    CHECK(xchash_remove_node(forward, long_node));
    CHECK(xchash_node_count(forward) == 3);
    CHECK(xchash_remove_node(forward, "missing"));
    CHECK(xchash_node_count(forward) == 3);

    /* 新节点加入后，原键若迁移，只能迁到新节点。 */
    CHECK(xchash_add_node(forward, "node-D"));
    for (i = 0; i < 1000; ++i) {
        char* previous;
        char* current;
        snprintf(key, sizeof(key), "request-%d", i);
        previous = (char*)xchash_get_node(reverse, key);
        current = (char*)xchash_get_node(forward, key);
        CHECK(previous != NULL && current != NULL &&
            (strcmp(previous, current) == 0 || strcmp(current, "node-D") == 0));
        xrtFree(previous);
        xrtFree(current);
    }
    CHECK(xchash_remove_node(forward, "node-D"));
    for (i = 0; i < 1000; ++i) {
        char* current;
        char* previous;
        snprintf(key, sizeof(key), "request-%d", i);
        current = (char*)xchash_get_node(forward, key);
        previous = (char*)xchash_get_node(reverse, key);
        CHECK(current != NULL && previous != NULL && strcmp(current, previous) == 0);
        xrtFree(current);
        xrtFree(previous);
    }

    CHECK(xchash_remove_node(forward, "node-A"));
    CHECK(xchash_remove_node(forward, "node-B"));
    CHECK(xchash_remove_node(forward, "node-C"));
    CHECK(xchash_node_count(forward) == 0 && xchash_vnode_count(forward) == 0);
    saved = (char*)xchash_get_node(forward, "any");
    CHECK(saved != NULL && strcmp(saved, "") == 0);
    xrtFree(saved);

    xchash_destroy(reverse);
    xchash_destroy(forward);
    CHECK(test_allocation_failure() == 0);
    CHECK(test_dual_growth_failure() == 0);
    CHECK(test_name_limit() == 0);
    CHECK(test_fixed_layout() == 0);
    CHECK(test_error_recovery() == 0);
    puts("consistent_hash C boundaries: PASS");
    return 0;
}
