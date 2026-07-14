#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
    fflush(stdout);
}

int main(void)
{
    xtstack stack = NULL;
    xtavltree tree = NULL;
    xtqueue queue = NULL;
    xtqueue waits = NULL;
    xtarray keys = NULL;
    int64 first = 10;
    int64 second = 20;
    int64 value = 0;
    const char* keyA = "a";
    const char* keyB = "b";
    const char* text = NULL;
    const char* hello = "hello";

    xrtInit();

    stack = xrtTypedStackCreate(xrtTypeInt());
    require_line(
        "stack",
        stack != NULL &&
        xrtTypedStackPush(stack, &first) &&
        xrtTypedStackPush(stack, &second) &&
        xrtTypedStackPop(stack, &value) && value == second);

    tree = xrtTypedAvlTreeCreate(xrtTypeString(), xrtTypeInt(), XRT_OBJMODE_SHARED);
    require_line(
        "avl",
        tree != NULL &&
        xrtTypedAvlTreeSet(tree, &keyB, &second) &&
        xrtTypedAvlTreeSet(tree, &keyA, &first) &&
        (keys = xrtTypedAvlTreeKeys(tree, XRT_OBJMODE_LOCAL)) != NULL &&
        xrtTypedArrayCount(keys) == 2u &&
        strcmp(*(const char**)xrtTypedArrayGet(keys, 1u), "a") == 0 &&
        strcmp(*(const char**)xrtTypedArrayGet(keys, 2u), "b") == 0);

    queue = xrtTypedQueueCreate(XRT_TYPED_QUEUE_MPMC, xrtTypeString(), 8u);
    require_line(
        "queue",
        queue != NULL &&
        xrtTypedQueueTryPush(queue, &hello) == XQUEUE_OK &&
        xrtTypedQueueTryPop(queue, &text) == XQUEUE_OK &&
        text != NULL && strcmp(text, hello) == 0);
    xrtTypeDropValue(xrtTypeString(), &text);

    waits = xrtTypedQueueCreate(XRT_TYPED_QUEUE_WAIT, xrtTypeInt(), 4u);
    require_line(
        "wait_queue",
        waits != NULL &&
        xrtTypedQueuePopTimeout(waits, &value, 1u) == XQUEUE_TIMEOUT &&
        xrtTypedQueueTryPush(waits, &second) == XQUEUE_OK &&
        xrtTypedQueuePop(waits, &value) == XQUEUE_OK && value == second);

    xrtTypedQueueRelease(waits);
    xrtTypedQueueRelease(queue);
    xrtTypedArrayRelease(keys);
    xrtTypedAvlTreeRelease(tree);
    xrtTypedStackRelease(stack);
    xrtUnit();
    return 0;
}
