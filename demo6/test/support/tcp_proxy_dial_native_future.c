/* Ported from XRT 604b7bcc. Only bootstrap and pinned clock units change.
 * Execute valid fixed replies and normal own-loopback lifecycle only. */
#ifdef TEST_PROXY_RUNTIME_PRODUCT
#define XRUNTIME_IMPLEMENTATION
#include "xruntime.h"
#else
#define XRT_IMPLEMENTATION
#include "xrt.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void testRequire(bool condition, const char* message)
{ if (!condition) { fprintf(stderr, "[FAIL] %s\n", message); exit(1); } }
#define testMemoryDebugDrain(message) testRequire(xrtMemDebugReset(), message)
#include "tcp_proxy_dial_peer.h"
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%u: ", __FILE__, (unsigned)__LINE__); testRequire(false, #c); } } while (0)

typedef struct FutureProxyScene {
    volatile int32 refs;
    xnetengine* engine;
    xnetresolver* resolver;
    xnetproxy* proxy;
    xfuture* future;
    xnetstream* stream;
    xatomic32 lookupProceed, lookupEntered;
    bool busy;
} FutureProxyScene;
static unsigned cases, snapshots, familyRefusals, budgets, failedBudgets, roundtrips;
static bool futureProxyCount(const void* data, size_t* count)
{
    const FutureProxyScene* s = data;
    if (!s || !count || __xrtAtomicRefLoad(&s->refs) <= 0) return false;
    *count = (size_t)__xrtAtomicRefLoad(&s->refs); return true;
}
static bool futureProxyTrace(const void* data, xrtownershipvisitor visit, ptr context)
{
    const FutureProxyScene* s = data; size_t count;
    if (!visit || !futureProxyCount(s, &count)) return false;
    if (s->engine && !visit(xrtNetEngineOwnership(s->engine), context)) return false;
    if (s->resolver && !visit(xrtNetResolverOwnership(s->resolver), context)) return false;
    if (s->proxy && !visit(xrtNetProxyOwnership(s->proxy), context)) return false;
    if (s->future && !visit(xrtFutureOwnership(s->future), context)) return false;
    return !s->stream || visit(xrtNetStreamOwnership(s->stream), context);
}
static const xrtownershipops futureProxyOps = {futureProxyCount, futureProxyTrace};
static FutureProxyScene* futureProxyHold(FutureProxyScene* s)
{ CHECK(xrtOwnershipRefRetain(&s->refs) > 0); return s; }
static void futureProxyDrop(const void* data)
{
    FutureProxyScene* s = (FutureProxyScene*)data;
    int32 refs = xrtOwnershipRefRelease(&s->refs); CHECK(refs >= 0);
    if (!refs) {
        CHECK(!s->engine && !s->resolver && !s->proxy && !s->future && !s->stream);
        xrtFree(s);
    }
}
static xnetaddrlist* futureProxyLookup(cstr host, xnetfamily family, ptr data)
{
    (void)host; (void)family; FutureProxyScene* s = data;
    xrtAtomic32Store(&s->lookupEntered, 1, XMEMORY_RELEASE);
    xdeadline due = xrtDeadlineAfter(UINT64_C(10000000));
    while (!xrtAtomic32Load(&s->lookupProceed, XMEMORY_ACQUIRE)) {
        CHECK(!xrtDeadlineExpired(due)); xrtThreadYield();
    }
    xnetaddr address; CHECK(xrtNetAddrLoopback(&address, XNET_FAMILY_IPV4, 0));
    return xrtNetAddrListCreate(&address, 1);
}
static const xnetresolverlookupownershipv1 futureProxyLookupPolicy = {
    sizeof(xnetresolverlookupownershipv1), futureProxyLookup, futureProxyDrop, &futureProxyOps
};
static const xrtownershipadapterv1* futureProxyResolve(FutureProxyScene* s, xrtownershipref ref,
    const xrtownershippreparationv1** prep)
{
    const xrtownershipquiescencev1* quiet[] = {
        xrtNetEngineOwnershipQuiescenceV1Get(), xrtNetResolverOwnershipQuiescenceV1Get(),
        xrtNetResolveOpOwnershipQuiescenceV1Get(), xrtNetStreamOwnershipQuiescenceV1Get(),
        xrtNetBufPoolOwnershipQuiescenceV1Get(), xrtNetDialOwnershipQuiescenceV1Get(),
        xrtNetProxyDialOwnershipQuiescenceV1Get(), xrtNetProxyDialFutureOwnershipQuiescenceV1Get(),
        xrtFutureOwnershipQuiescenceV1Get(), xrtCancelOwnershipQuiescenceV1Get(),
        xrtCancelWatchOwnershipQuiescenceV1Get()
    };
    for (size_t i = 0; i < sizeof(quiet) / sizeof(*quiet); ++i)
        if (ref.Ops == quiet[i]->Ops && quiet[i]->Busy(ref.Data)) { s->busy = true; return NULL; }
    const xnetruntimeownershipv1* runtimes[] = {
        xrtNetStreamRuntimeOwnershipV1Get(), xrtNetDialRuntimeOwnershipV1Get(),
        xrtNetProxyDialRuntimeOwnershipV1Get()
    };
    const xnetresolverlookupownershipv1* lookups[] = {&futureProxyLookupPolicy};
    const xnetresolveownershipv1* requests[] = {xrtNetDialResolverPolicyV1Get()};
    const xnetdialownershipv1* transports[] = {xrtNetProxyDialTransportOwnershipV1Get()};
    const xnetstreameventsownershipv1* events[] = {xrtNetProxyDialStreamOwnershipV1Get()};
    const xnetproxydialownershipv1* done[] = {xrtNetProxyDialFutureDialPolicyV1Get()};
    const xfutureproducerownershipv1* producers[] = {xrtNetProxyDialFutureProducerPolicyV1Get()};
    const xcancelwatchownershipv1* cancels[] = {xrtNetProxyDialFutureCancelPolicyV1Get()};
    const xfuturepayloadownershipv1* payloads[] = {xrtNetProxyDialFuturePayloadPolicyV1Get()};
    xfutureownershipadmissionv1 admission = {0}; admission.size = sizeof(admission);
    admission.ProducerPolicies = producers; admission.ProducerPolicyCount = 1;
    admission.CancelWatchPolicies = cancels; admission.CancelWatchPolicyCount = 1;
    admission.PayloadPolicies = payloads; admission.PayloadPolicyCount = 1;
    const xrtownershipadapterv1* a = xrtNetEngineOwnershipAdapterV3(ref, NULL, 0, NULL, 0, NULL, 0, runtimes, 3, prep);
    if (!a) a = xrtNetResolverOwnershipAdapterV1(ref, lookups, 1, prep);
    if (!a) a = xrtNetResolveOpOwnershipAdapterV1(ref, requests, 1, prep);
    if (!a) a = xrtNetDialOwnershipAdapterV1(ref, transports, 1, prep);
    if (!a) a = xrtNetStreamOwnershipAdapterV2(ref, events, 1, prep);
    if (!a) a = xrtNetBufPoolOwnershipAdapterV1(ref, prep);
    if (!a) a = xrtNetProxyDialOwnershipAdapterV1(ref, done, 1, prep);
    if (!a && ref.Ops == xrtNetProxyDialFutureOwnershipQuiescenceV1Get()->Ops) {
        const xrtownershippreparationv1* unchanged = (const xrtownershippreparationv1*)s;
        CHECK(!xrtNetDialFutureOwnershipAdapterV1(ref, &unchanged) &&
            unchanged == (const xrtownershippreparationv1*)s);
        ++familyRefusals;
    }
    if (!a) a = xrtNetProxyDialFutureOwnershipAdapterV1(ref, prep);
    if (!a) a = xrtFutureOwnershipAdapterV4(ref, &admission, prep);
    if (!a) a = xrtCancelOwnershipAdapterV2(ref, cancels, 1, prep);
    if (!a) a = xrtCancelWatchOwnershipAdapterV1(ref, cancels, 1, prep);
    if (!a) a = xrtNetProxyOwnershipAdapterV1(ref);
    if (!a) a = xrtNetAddrListOwnershipAdapterV1(ref);
    if (!a) a = xrtErrorOwnershipAdapterV1(ref);
    return a;
}
static bool futureProxyAdmit(xrtownershipref ref, ptr data)
{
    if (ref.Ops == &futureProxyOps) return true;
    const xrtownershippreparationv1* prep = NULL;
    return futureProxyResolve(data, ref, &prep) != NULL;
}
static void futureProxySnapshot(FutureProxyScene* s)
{
    xrtownershipref root = {s, &futureProxyOps}; xdeadline due = xrtDeadlineAfter(UINT64_C(10000000));
    for (;;) {
        xrtownershipscope freeze = {0};
        if (!xrtOwnershipFreezeTryBegin(&freeze)) { CHECK(!xrtDeadlineExpired(due)); xrtThreadYield(); continue; }
        s->busy = xrtNetEngineOwnershipQuiescenceV1Get()->Busy(s->engine);
        xrtownershipsnapshot* snapshot = NULL;
        bool ok = !s->busy && xrtOwnershipSnapshotCreate(&root, 1, &root, 1, futureProxyAdmit, s, &snapshot);
        if (ok) {
            for (size_t i = 0; i < xrtOwnershipSnapshotNodeCount(snapshot); ++i) {
                xrtownershipnode node; CHECK(xrtOwnershipSnapshotNode(snapshot, i, &node));
                CHECK(!node.Reachable);
            }
            xrtOwnershipSnapshotDestroy(snapshot); CHECK(xrtOwnershipScopeEnd(&freeze)); ++snapshots; return;
        }
        CHECK(s->busy); xrtClearError();
        CHECK(xrtOwnershipScopeEnd(&freeze) && !xrtDeadlineExpired(due)); xrtThreadYield();
    }
}
typedef struct FutureProxyGate { xatomic32 entered, proceed; } FutureProxyGate;
typedef struct FutureProxyRead { xnetstream* stream; xatomic32 done; char bytes[11]; } FutureProxyRead;
static void futureProxyRead(xnetworker* worker, ptr data)
{
    (void)worker; FutureProxyRead* read = data;
    CHECK(xrtNetStreamRead(read->stream, read->bytes, sizeof(read->bytes)) == sizeof(read->bytes));
    CHECK(!memcmp(read->bytes, "prefaceping", sizeof(read->bytes)));
    xrtAtomic32Store(&read->done, 1, XMEMORY_RELEASE);
}
static void futureProxyGate(xnetworker* worker, ptr data)
{
    (void)worker; FutureProxyGate* gate = data;
    xrtAtomic32Store(&gate->entered, 1, XMEMORY_RELEASE); xdeadline due = xrtDeadlineAfter(UINT64_C(10000000));
    while (!xrtAtomic32Load(&gate->proceed, XMEMORY_ACQUIRE)) { CHECK(!xrtDeadlineExpired(due)); xrtThreadYield(); }
}
static bool futureProxyCase(xnetportbackend backend, unsigned mode, int64 budget)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    FutureProxyScene* s = xrtCalloc(1, sizeof(*s)); CHECK(s); s->refs = 1;
    xrtAtomic32Init(&s->lookupEntered, 0);
    xrtAtomic32Init(&s->lookupProceed, budget < 0 && mode != 4 ? 1 : 0);
    testproxycontext peer = {0}; peer.Mode = mode == 2 || mode == 3 || mode == 4 ? TEST_PROXY_STALL : TEST_PROXY_SUCCESS;
    peer.RequireAuth = mode == 1; peer.Stage = TEST_PROXY_DIAL_HTTP_CONNECT ? TEST_PROXY_CONNECT : TEST_PROXY_GREETING;
    xnetengineconfig ec; xrtNetEngineConfigInit(&ec); ec.Backend = backend; ec.Workers = 1; ec.NodeCacheBytes = 0;
    xnetengine* peerEngine = xrtNetEngineCreate(&ec); CHECK(peerEngine && xrtNetEngineStart(peerEngine));
    s->engine = xrtNetEngineCreate(&ec); CHECK(s->engine && xrtNetEngineStart(s->engine));
    xnetlistenconfig lc; xrtNetListenConfigInit(&lc); CHECK(xrtNetAddrLoopback(&lc.Address, XNET_FAMILY_IPV4, 0));
    xnetlistenerevents le = {testProxyDialAccept, NULL, testProxyDialListenerClose};
    xnetstreamevents se = {NULL, testProxyDialServerRead, NULL, NULL, NULL, NULL, NULL};
    xnetlistener* listener = xrtNetListen(peerEngine, &lc, &le, &se, &peer); CHECK(listener);
    xnetaddr address; CHECK(xrtNetListenerLocal(listener, &address));
    xnetresolverconfig rc; xrtNetResolverConfigInit(&rc); rc.Workers = 1; rc.CacheEntries = 0;
    s->resolver = xrtNetResolverCreateOwnedV1(&rc, futureProxyHold(s), &futureProxyLookupPolicy); CHECK(s->resolver);
    xnetproxyconfig pc; xrtNetProxyConfigInit(&pc); pc.Type = TEST_PROXY_DIAL_HTTP_CONNECT ? XNET_PROXY_HTTP_CONNECT : XNET_PROXY_SOCKS5;
    pc.Host = XRT_STR_LITERAL("proxy.test"); pc.Port = address.Port;
    if (mode == 1) { pc.Username = XRT_BYTES_LITERAL("user"); pc.Password = XRT_BYTES_LITERAL("password"); }
    s->proxy = xrtNetProxyCreate(&pc); CHECK(s->proxy);
    xnetproxydialconfig dc; xrtNetProxyDialConfigInit(&dc); dc.ReceiveLimit = 512;
    dc.Transport.Stream.ReadLimit = 1024; dc.Transport.Stream.ReadSize = 64; dc.Timeout = mode == 3 ? UINT64_C(100000) : UINT64_C(10000000);
    FutureProxyGate gate = {0}; xrtAtomic32Init(&gate.entered, 0); xrtAtomic32Init(&gate.proceed, 0);
    if (budget >= 0) { CHECK(xrtNetEnginePost(s->engine, 0, futureProxyGate, &gate)); testProxyDialWait(&gate.entered, 1, "Future constructor gate"); }
    xrtownershipscope mutation = {0}; CHECK(xrtOwnershipMutationBegin(&mutation));
    if (budget >= 0) CHECK(xrtMemDebugFailAfter((size_t)budget));
    s->future = xrtNetProxyDialAsync(s->engine, s->resolver, s->proxy, "origin.test", 8443, &dc, NULL, NULL);
    bool hit = budget >= 0 && xrtMemDebugFailTriggered();
    if (budget >= 0) {
        xrtMemDebugFailClear(); ++budgets;
        if (!s->future) { CHECK(hit && xrtErrorKind(xrtGetError()) == XERR_MEMORY); xrtClearError(); ++failedBudgets; }
        xrtAtomic32Store(&s->lookupProceed, 1, XMEMORY_RELEASE);
        xrtAtomic32Store(&gate.proceed, 1, XMEMORY_RELEASE);
    } else CHECK(s->future);
    CHECK(xrtOwnershipScopeEnd(&mutation));
    if (!s->future) goto cleanup;
    if (mode == 2) {
        testProxyDialWait(&peer.Accepted, 1, "Future peer accept"); futureProxySnapshot(s);
        CHECK(xrtFutureState(s->future) == XFUTURE_PENDING && xrtFutureCancel(s->future));
    } else if (mode == 4) {
        testProxyDialWait(&s->lookupEntered, 1, "Future blocked lookup"); CHECK(xrtFutureCancel(s->future));
    }
    CHECK(xrtFutureWaitFor(s->future, UINT64_C(10000000)) == XWAIT_OK);
    CHECK(xrtFutureState(s->future) == (mode == 2 || mode == 4 ? XFUTURE_CANCELLED : mode == 3 ? XFUTURE_FAILED : XFUTURE_RESOLVED));
    xrtAtomic32Store(&s->lookupProceed, 1, XMEMORY_RELEASE);
    if (mode == 0 || mode == 1 || mode == 5) {
        xnetstream* value = xrtFutureValue(s->future);
        CHECK(value && xrtNetStreamState(value) == XNET_STREAM_OPEN);
        CHECK(peer.AuthSeen == (mode == 1) && !strcmp(peer.TargetHost, "origin.test") && peer.TargetPort == 8443);
        if (mode == 5) {
            s->stream = xrtNetStreamRef(value); CHECK(s->stream);
            xrtFutureDestroy(s->future); s->future = NULL;
            CHECK(xrtNetStreamState(s->stream) == XNET_STREAM_OPEN);
        }
        CHECK(xrtNetStreamSend(value, "ping", 4) == XNET_RESULT_OK);
        xdeadline readDue = xrtDeadlineAfter(UINT64_C(10000000));
        while (xrtNetStreamAvailable(value) < 11) { CHECK(!xrtDeadlineExpired(readDue)); xrtThreadYield(); }
        FutureProxyRead read = {0}; read.stream = value; xrtAtomic32Init(&read.done, 0);
        CHECK(xrtNetEnginePost(s->engine, 0, futureProxyRead, &read));
        testProxyDialWait(&read.done, 1, "Future tunnel roundtrip"); ++roundtrips;
    }
    futureProxySnapshot(s);
cleanup:
    if (s->future && xrtFutureState(s->future) == XFUTURE_RESOLVED) {
        xnetstream* value = xrtFutureValue(s->future); CHECK(xrtNetStreamAbort(value));
        testProxyDialWaitClosed(value, "Future result close");
    }
    if (s->stream) { CHECK(xrtNetStreamAbort(s->stream)); testProxyDialWaitClosed(s->stream, "promoted result close"); }
    if (peer.Server) { CHECK(xrtNetStreamAbort(peer.Server)); testProxyDialWaitClosed(peer.Server, "Future peer close"); }
    CHECK(xrtNetListenerClose(listener)); testProxyDialWait(&peer.ListenerClosed, 1, "Future listener close");
    xrtNetStreamDestroy(peer.Server); xrtNetListenerDestroy(listener); CHECK(xrtNetEngineDestroy(peerEngine));
    xrtNetStreamDestroy(s->stream); s->stream = NULL; xrtFutureDestroy(s->future); s->future = NULL;
    xrtNetProxyRelease(s->proxy); s->proxy = NULL; CHECK(xrtNetResolverDestroy(s->resolver)); s->resolver = NULL;
    xdeadline due = xrtDeadlineAfter(UINT64_C(10000000));
    while (!xrtNetEngineDestroy(s->engine)) { xrtClearError(); CHECK(!xrtDeadlineExpired(due)); xrtThreadYield(); }
    s->engine = NULL; CHECK(__xrtAtomicRefLoad(&s->refs) == 1); futureProxyDrop(s); xrtClearError();
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    ++cases; return hit;
}
int main(int argc, char** argv)
{
    xnetportbackend backend = XNET_PORT_SELECT;
    if (argc > 1 && !strcmp(argv[1], "native")) {
#ifdef _WIN32
        backend = XNET_PORT_IOCP;
#else
        backend = XNET_PORT_EPOLL;
#endif
    }
    CHECK(xrtMemDebugEnable(true));
    for (unsigned mode = 0; mode < 6; ++mode) (void)futureProxyCase(backend, mode, -1);
    unsigned point = 0;
    for (; point < 64; ++point) if (!futureProxyCase(backend, 0, (int64)point)) break;
    CHECK(point < 64 && failedBudgets > 0 && failedBudgets < budgets && familyRefusals > 0 && roundtrips == 4);
    testMemoryDebugDrain("Proxy Future graphs converge");
    printf("[PASS] Proxy Future backend=%d http=%d cases=%u snapshots=%u family-refusals=%u budgets=%u failures=%u roundtrips=%u literal loopback balanced\n",
        (int)backend, TEST_PROXY_DIAL_HTTP_CONNECT, cases, snapshots, familyRefusals, budgets, failedBudgets, roundtrips);
    return 0;
}
