/* Resident mapper ownership audit; generated C is preincluded by the gate. */
#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
static unsigned executions, faults;
static unsigned transaction(xnetaddrlist* list, xnetudppacket* packet, unsigned mode, int fault)
{
    xmemdebugsnapshot before, armed, after;
    xfuture* future = NULL;
    xpromise* promise;
    xfutureresult input = {0}, output;
    unsigned allocations;
    xrtMemDebugSnapshot(&before);
    promise = xrtPromiseCreate(&future, NULL); assert(promise && future);
    input.State = XFUTURE_RESOLVED; input.Value = mode == 2 ? (void*)packet : (void*)list;
    xrtMemDebugSnapshot(&armed);
    if (fault >= 0) assert(xrtMemDebugFailAfter((uint64)fault));
    if (mode == 2) xlir_datagram_map_packet(&input, promise, NULL);
    else xlir_datagram_map_addresses(&input, promise, (void*)(uintptr_t)(mode ? 9999 : 8123));
    if (fault >= 0) { assert(xrtMemDebugFailTriggered()); ++faults; }
    xrtMemDebugFailClear();
    xrtMemDebugSnapshot(&after); allocations = (unsigned)(after.AllocCount-armed.AllocCount);
    assert(xrtFutureResult(future, &output));
    if (fault >= 0) {
        if (output.State != XFUTURE_FAILED) fprintf(stderr, "mode=%u fault=%d state=%d\n", mode, fault, output.State);
        assert(output.State == XFUTURE_FAILED && output.Error && xrtErrorKind(output.Error) == XERR_MEMORY && output.Value == NULL);
    } else {
        xvalue* value;
        assert(output.State == XFUTURE_RESOLVED && output.Value && !output.Error);
        value = *(xvalue**)output.Value; assert(value);
        if (mode == 2) assert(xrtNetUdpPacketSize((xnetudppacket*)xlir_net_packet_borrow_handle(value)) == 1);
        else assert(xrtNetAddrListGet((xnetaddrlist*)xlir_net_addresses_borrow_handle(value), 0)->Port == (mode ? 9999 : 8123));
    }
    xrtClearError(); xrtPromiseDestroy(promise); xrtFutureDestroy(future); assert(!xrtGetError());
    assert(xrtNetAddrListGet(list, 0)->Port == 8123 && xrtNetUdpPacketSize(packet) == 1);
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
    assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount && after.UseAfterFreeCount == before.UseAfterFreeCount);
    ++executions; return allocations;
}
int main(void)
{
    xnetaddrlist* list;
    xnetudppacket* packet;
    void *sender, *receiver;
    unsigned mode, round, failure, allocations[3];
    xmemdebugsnapshot initial, final;
    assert(xrtMemDebugEnable(true));
    assert(xl6_net_runtime_init());
    sender = xl6_xrt_udp_bind("127.0.0.1", 0); receiver = xl6_xrt_udp_bind("127.0.0.1", 0); assert(sender && receiver);
    assert(xl6_xrt_udp_send_text(sender, "127.0.0.1", xl6_xrt_udp_local_port(receiver), "x"));
    packet = xl6_xrt_udp_recv(receiver, 8000); assert(packet);
    xl6_xrt_udp_destroy(sender); xl6_xrt_udp_destroy(receiver); xl6_net_runtime_unit(); assert(!xrtGetError());
    list = xrtNetResolve("127.0.0.1", 8123, XNET_FAMILY_IPV4); assert(list && xrtNetAddrListCount(list));
    xrtMemDebugSnapshot(&initial);
    for (mode = 0; mode < 3; ++mode) {
        allocations[mode] = transaction(list, packet, mode, -1);
        assert(allocations[mode] > 0 && allocations[mode] < 40);
        printf("mapper mode=%u allocations=%u\n", mode, allocations[mode]);
    }
    for (round = 0; round < 20; ++round) for (mode = 0; mode < 3; ++mode)
        for (failure = 0; failure < allocations[mode]; ++failure) transaction(list, packet, mode, (int)failure);
    xrtMemDebugSnapshot(&final); assert(initial.LiveCount == final.LiveCount && initial.LiveBytes == final.LiveBytes);
    xrtNetAddrListDestroy(list); xrtNetUdpPacketDestroy(packet); assert(!xrtGetError());
    printf("datagram mappers: %u transactions, %u allocation faults; same-port ref, changed-port copy, packet ref; exact per-transaction memory balance\n", executions, faults);
    return 0;
}
