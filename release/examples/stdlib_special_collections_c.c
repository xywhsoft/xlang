#define XRUNTIME_MODULE_TYPED_STACK
#define XRUNTIME_MODULE_TYPED_TREE
#define XRUNTIME_MODULE_TYPED_QUEUE_SPSC
#define XRUNTIME_MODULE_TYPED_QUEUE_MPSC
#define XRUNTIME_MODULE_TYPED_QUEUE_MPMC
#define XRT_MODULE_CHANNEL
#define XRUNTIME_MODULE_RUNTIME_TYPE_STRING
#include "xruntime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* 验证一个示例步骤，并保持与 xlang 示例相同的输出。 */
static void require_line(const char* sName, int bPassed)
{
	if ( !bPassed ) {
		fprintf(stderr, "%s=fail\n", sName);
		exit(1);
	}

	printf("%s=ok\n", sName);
	fflush(stdout);
}



/* 对照演示新版 XRT 类型集合与等待队列接口。 */
int main(void)
{
	xtypedstack* pStack = NULL;
	xtypedtree* pTree = NULL;
	xtypedspscqueue* pSPSC = NULL;
	xtypedmpscqueue* pMPSC = NULL;
	xtypedmpmcqueue* pMPMC = NULL;
	xchannel* pWaitQueue = NULL;
	const void* pFirstKey = NULL;
	const void* pSecondKey = NULL;
	ptr pReceived = NULL;
	int64 iFirst = 10;
	int64 iSecond = 20;
	int64 iValue = 0;
	const char* sKeyA = "a";
	const char* sKeyB = "b";
	const char* sText = NULL;
	const char* sHello = "hello";

	/* 类型栈直接复用类型数组的值所有权。 */
	pStack = xrtTypedStackCreate(xrtTypeInt64());
	require_line(
		"stack",
		(pStack != NULL) &&
		xrtTypedStackPush(pStack, &iFirst) &&
		xrtTypedStackPush(pStack, &iSecond) &&
		xrtTypedStackPop(pStack, &iValue) &&
		(iValue == iSecond)
	);

	/* 类型树按键类型排序，并允许零分配地遍历内部规范键。 */
	pTree = xrtTypedTreeCreate(xrtTypeString(), xrtTypeInt64());
	require_line(
		"avl",
		(pTree != NULL) &&
		xrtTypedTreeSet(pTree, &sKeyB, &iSecond) &&
		xrtTypedTreeSet(pTree, &sKeyA, &iFirst) &&
		(xrtTypedTreeCount(pTree) == 2u) &&
		(xrtTypedTreeFirst(pTree, &pFirstKey) != NULL) &&
		(pFirstKey != NULL) &&
		(strcmp(*(const char* const*)pFirstKey, "a") == 0) &&
		(xrtTypedTreeUpperBound(pTree, pFirstKey, &pSecondKey) != NULL) &&
		(pSecondKey != NULL) &&
		(strcmp(*(const char* const*)pSecondKey, "b") == 0)
	);

	/* 并发拓扑由类型明确表达，避免在运行期携带队列模式分支。 */
	pSPSC = xrtTypedSPSCQueueCreate(xrtTypeInt64(), 4u);
	iValue = 0;
	require_line(
		"spsc",
		(pSPSC != NULL) &&
		(xrtTypedSPSCQueueTryPush(pSPSC, &iFirst) == XQUEUE_OK) &&
		(xrtTypedSPSCQueueTryPop(pSPSC, &iValue) == XQUEUE_OK) &&
		(iValue == iFirst)
	);

	pMPSC = xrtTypedMPSCQueueCreate(xrtTypeInt64(), 4u);
	iValue = 0;
	require_line(
		"mpsc",
		(pMPSC != NULL) &&
		(xrtTypedMPSCQueueTryPush(pMPSC, &iSecond) == XQUEUE_OK) &&
		(xrtTypedMPSCQueueTryPop(pMPSC, &iValue) == XQUEUE_OK) &&
		(iValue == iSecond)
	);

	pMPMC = xrtTypedMPMCQueueCreate(xrtTypeString(), 8u);
	require_line(
		"mpmc",
		(pMPMC != NULL) &&
		(xrtTypedMPMCQueueTryPush(pMPMC, &sHello) == XQUEUE_OK) &&
		(xrtTypedMPMCQueueTryPop(pMPMC, &sText) == XQUEUE_OK) &&
		(sText != NULL) &&
		(strcmp(sText, sHello) == 0)
	);
	xrtTypeDropValue(xrtTypeString(), &sText);

	/* Channel 提供有界阻塞、超时和关闭语义。 */
	pWaitQueue = xrtChannelCreate(4u);
	require_line(
		"wait_queue",
		(pWaitQueue != NULL) &&
		(xrtChannelRecvFor(pWaitQueue, &pReceived, 1000u) == XWAIT_TIMEOUT) &&
		(xrtChannelSend(pWaitQueue, &iSecond) == XWAIT_OK) &&
		(xrtChannelRecv(pWaitQueue, &pReceived) == XWAIT_OK) &&
		(pReceived == &iSecond) &&
		(*(int64*)pReceived == iSecond)
	);

	xrtChannelDestroy(pWaitQueue);
	xrtTypedMPMCQueueDestroy(pMPMC);
	xrtTypedMPSCQueueDestroy(pMPSC);
	xrtTypedSPSCQueueDestroy(pSPSC);
	xrtTypedTreeDestroy(pTree);
	xrtTypedStackDestroy(pStack);
	return 0;
}
