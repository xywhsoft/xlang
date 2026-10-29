/* Native fixture: XRT 40704d47 TCP runtime/pool/bytes/buffer ownership.
 * Adapted ONLY for the pinned millisecond/XL6_DeadlineV1 ABI and standalone include.
 * Actual RC, queue, payload, preparation and OOM semantics are unchanged. */
#define XRT_IMPLEMENTATION
#include "release/include/xl_clock.h"
#include "xrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void testRequire(bool condition, const char* message)
{
    if (!condition) { fprintf(stderr, "[FAIL] %s\n", message); exit(1); }
}
#define testMemoryDebugDrain(Message) testRequire(xrtMemDebugReset(), (Message))

#define CHECK(Condition) do { if (!(Condition)) { \
	fprintf(stderr, "%s:%u: ", __FILE__, (unsigned)__LINE__); testRequire(false, #Condition); } } while (0)
typedef struct TcpGraph {
	xnetengine* engine;
	xnetlistener* listener;
	xnetstream* client;
	xrtownershipref refs[16];
	const xrtownershipadapterv1* adapters[16];
	const xrtownershippreparationv1* preparations[5];
	xatomic32 timers_ready;
	xatomic32 payload_ready;
	xnetbytes* owned_bytes; /* actual caller creator, not a borrowed graph alias */
	unsigned payload_mode;
	size_t nodes;
} TcpGraph;
static unsigned graph_cases, timer_cases, future_cases, error_cases, refusal_cases, buffered_cases, retained_write_cases, allocation_budgets, allocation_failures;

static void freeze_begin(xrtownershipscope* scope)
{
	XL6_DeadlineV1 deadline = xl6DeadlineAfterMillisecondsV1(5000);
	while (!xrtOwnershipFreezeTryBegin(scope)) {
		CHECK(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
	}
}
static bool admit(xrtownershipref ref, ptr data)
{
	TcpGraph* graph = data;
	const xrtownershippreparationv1* preparation = NULL;
	const xnetruntimeownershipv1* runtimes[] = {
		xrtNetStreamRuntimeOwnershipV1Get(), xrtNetListenerRuntimeOwnershipV1Get()};
	if (ref.Ops == xrtNetEngineOwnership(graph->engine).Ops)
		return xrtNetEngineOwnershipAdapterV3(ref, NULL, 0, NULL, 0, NULL, 0, runtimes, 2, &preparation) != NULL;
	if (ref.Ops == xrtNetStreamRuntimeOwnershipV1Get()->Ops)
		return xrtNetStreamOwnershipAdapterV1(ref, &preparation) != NULL;
	if (ref.Ops == xrtNetListenerRuntimeOwnershipV1Get()->Ops)
		return xrtNetListenerOwnershipAdapterV1(ref, &preparation) != NULL;
	if (ref.Ops == xrtNetBufPoolOwnershipQuiescenceV1Get()->Ops)
		return xrtNetBufPoolOwnershipAdapterV1(ref, &preparation) != NULL;
	return xrtNetBytesOwnershipAdapterV1(ref) != NULL || xrtErrorOwnershipAdapterV1(ref) != NULL;
}
/* The caller's real creator slots and, later, real plan pins are declared
 * internal exactly once each. Borrowed aliases never subtract a reference. */
static void snapshot(TcpGraph* graph, bool pinned, xrtownershipscope* freeze)
{
	XL6_DeadlineV1 deadline = xl6DeadlineAfterMillisecondsV1(5000);
	for (;;) {
		freeze_begin(freeze);
		bool busy = xrtNetEngineOwnershipQuiescenceV1Get()->Busy(graph->engine);
		for (size_t i = 1; !busy && i < 4; ++i) {
			if (!graph->refs[i].Data) continue;
			const xrtownershipquiescencev1* policy = i == 1 ?
				xrtNetListenerOwnershipQuiescenceV1Get() : xrtNetStreamOwnershipQuiescenceV1Get();
			busy = policy->Busy(graph->refs[i].Data);
		}
		if (!busy) {
			xrtownershipref slots[20] = {graph->refs[0], graph->refs[1], graph->refs[2]};
			size_t creators = 3;
			if (graph->owned_bytes) slots[creators++] = xrtNetBytesOwnership(graph->owned_bytes);
			if (pinned) for (size_t i = 0; i < graph->nodes; ++i) slots[creators+i] = graph->refs[i];
			xrtownershipsnapshot* result = NULL;
			if (xrtOwnershipSnapshotCreate(graph->refs, pinned ? graph->nodes : 3, slots, pinned ? creators+graph->nodes : creators, admit, graph, &result)) {
				size_t discovered = xrtOwnershipSnapshotNodeCount(result);
				for (size_t i = 0; i < discovered; ++i) {
					xrtownershipnode node;
					CHECK(xrtOwnershipSnapshotNode(result, i, &node));
					CHECK(!node.Reachable);
					if (!graph->refs[3].Data && node.Reference.Ops == xrtNetStreamRuntimeOwnershipV1Get()->Ops &&
						node.Reference.Data != graph->client) graph->refs[3] = node.Reference;
					{
						bool known = false;
						for (size_t j = 0; j < graph->nodes; ++j)
							if (graph->refs[j].Data == node.Reference.Data && graph->refs[j].Ops == node.Reference.Ops) known = true;
						if (!known) {
							/* Ordinary close can produce a fresh immutable native IO
							 * diagnostic. Rebuild, admit and pin it from the ACTUAL
							 * new graph, never ignore its edge or reuse an old proof. */
							CHECK(graph->nodes < 16);
							const xrtownershipadapterv1* adapter = xrtNetBytesOwnershipAdapterV1(node.Reference);
							if (!adapter) adapter = xrtErrorOwnershipAdapterV1(node.Reference);
							CHECK(adapter);
							if (pinned) CHECK(adapter->Hold(node.Reference.Data) && adapter->Claim(node.Reference.Data, graph));
							graph->adapters[graph->nodes] = adapter; graph->refs[graph->nodes++] = node.Reference;
						}
					}
				}
				if (!pinned) CHECK(discovered == graph->nodes);
				xrtOwnershipSnapshotDestroy(result); return;
			}
			xrtClearError();
		}
		CHECK(xrtOwnershipScopeEnd(freeze));
		CHECK(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
	}
}
static void balanced(const xmemdebugsnapshot* before)
{
	xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
	CHECK(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
	CHECK(after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount);
	CHECK(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
		before->UseAfterFreeCount == after.UseAfterFreeCount && before->OverflowCount == after.OverflowCount &&
		before->UnderflowCount == after.UnderflowCount);
}
static void create(TcpGraph* graph, xnetportbackend backend, bool future)
{
	graph->nodes = 5;
	xnetengineconfig engine; xrtNetEngineConfigInit(&engine);
	engine.Backend = backend; engine.Workers = 1; engine.NodeCacheBytes = 0;
	graph->engine = xrtNetEngineCreate(&engine);
	CHECK(graph->engine && xrtNetEngineStart(graph->engine));
	graph->refs[4] = xrtNetBufPoolOwnership(graph->engine->Workers[0].BufferPool);
	xnetlistenconfig config; xrtNetListenConfigInit(&config);
	CHECK(xrtNetAddrLoopback(&config.Address, XNET_FAMILY_IPV4, 0));
	config.Stream.ReadMode = XNET_STREAM_READ_PROBE;
	config.Stream.ReadSize = 1024; config.Stream.ReadLimit = 2048;
	config.Stream.WriteLimit = 16u*1024u*1024u;
	graph->listener = xrtNetListen(graph->engine, &config, NULL, NULL, NULL);
	CHECK(graph->listener);
	if (future) {
		xfuture* pending = xrtNetListenerAcceptAsync(graph->listener);
		CHECK(pending);
		xrtownershipscope freeze = {0}; XL6_DeadlineV1 deadline = xl6DeadlineAfterMillisecondsV1(5000);
		for (;;) {
			freeze_begin(&freeze);
			if (!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(graph->engine)) break;
			CHECK(xrtOwnershipScopeEnd(&freeze)); CHECK(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
		}
		xrtownershipref ref = xrtNetListenerOwnership(graph->listener); size_t count = 123;
		CHECK(ref.Ops->Count(ref.Data, &count) && count == 3);
		CHECK(!xrtFutureOwnershipAdapterV1(xrtFutureOwnership(pending), NULL, 0));
		CHECK(xrtOwnershipScopeEnd(&freeze));
		CHECK(xrtFutureCancel(pending)); xrtFutureDestroy(pending); ++future_cases;
	}
	xnetaddr address; CHECK(xrtNetListenerLocal(graph->listener, &address));
	graph->client = xrtNetStreamConnect(graph->engine, &address, 0, &config.Stream, NULL, NULL);
	CHECK(graph->client);
	XL6_DeadlineV1 deadline = xl6DeadlineAfterMillisecondsV1(5000);
	for (;;) {
		xnetlistenerstats stats; CHECK(xrtNetListenerStats(graph->listener, &stats));
		if (stats.QueuedAccepts == 1 && xrtNetStreamState(graph->client) == XNET_STREAM_OPEN) break;
		CHECK(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
	}
	graph->refs[0] = xrtNetEngineOwnership(graph->engine);
	graph->refs[1] = xrtNetListenerOwnership(graph->listener);
	graph->refs[2] = xrtNetStreamOwnership(graph->client);
}
/* Certify the very SAME pre-held references consumed by resident TCP timer
 * callbacks. This controlled long timer exercises the real heap, not a mock
 * count. Cancellation is the ordinary lifecycle path; nothing is transmitted. */
static void add_timers(xnetworker* worker, ptr data)
{
	TcpGraph* graph = data;
	const xnetruntimeownershipv1* stream = xrtNetStreamRuntimeOwnershipV1Get();
	const xnetruntimeownershipv1* listener = xrtNetListenerRuntimeOwnershipV1Get();
	CHECK(xrtNetStreamRef(graph->client));
	graph->client->ConnectTimer = xrtNetEngineAfter(graph->engine, xrtNetWorkerIndex(worker), UINT64_C(3600000000),
		stream->OwnedTimers[0], graph->client);
	CHECK(graph->client->ConnectTimer && __xrtNetEngineRuntimeTimer(worker, graph->client->ConnectTimer, stream));
	CHECK(xrtNetListenerRef(graph->listener));
	graph->listener->AcceptRetryTimer = xrtNetEngineAfter(graph->engine, xrtNetWorkerIndex(worker), UINT64_C(3600000000),
		listener->OwnedTimers[0], graph->listener);
	CHECK(graph->listener->AcceptRetryTimer && __xrtNetEngineRuntimeTimer(worker, graph->listener->AcceptRetryTimer, listener));
	xrtAtomic32Store(&graph->timers_ready, 1, XMEMORY_RELEASE);
}
/* Ordinary large binary data, deliberately left pending by normal receive
 * backpressure. No invalid traffic: the receiver simply has a small buffer. */
#define PAYLOAD_SIZE (1024u*1024u)
static void send_payload(xnetworker* worker, ptr data)
{
	TcpGraph* graph = data; CHECK(xrtNetWorkerEngine(worker) == graph->engine);
	CHECK(xrtNetSocketSet(xrtNetStreamSocket(graph->client), XNET_OPTION_SEND_BUFFER, 4096));
	CHECK(xrtNetSocketSet(xrtNetStreamSocket((xnetstream*)graph->refs[3].Data), XNET_OPTION_RECEIVE_BUFFER, 4096));
	if (graph->payload_mode == 2) { /* main-thread copying command after setup */
		xrtAtomic32Store(&graph->payload_ready, 1, XMEMORY_RELEASE); return;
	}
	uint8* payload = NULL;
	if (!graph->owned_bytes) {
		payload = xrtMalloc(PAYLOAD_SIZE); CHECK(payload); memset(payload, 'X', PAYLOAD_SIZE);
		memcpy(payload, "A\0B", 3);
	}
	xnetresult result;
	if (graph->payload_mode == 1) result = xrtNetStreamSend(graph->client, payload, PAYLOAD_SIZE);
	else if (graph->payload_mode == 3) {
		result = xrtNetStreamSendTake(graph->client, payload, PAYLOAD_SIZE);
		if (result == XNET_RESULT_OK) payload = NULL;
	} else if (graph->payload_mode == 4)
		result = xrtNetStreamSendBytes(graph->client, graph->owned_bytes, 0, PAYLOAD_SIZE);
	else {
		xnetbuf buffer; CHECK(xrtNetBufInit(&buffer, NULL));
		if (graph->payload_mode == 5) {
			CHECK(xrtNetBufAppend(&buffer, payload, PAYLOAD_SIZE/2));
			CHECK(xrtNetBufAppend(&buffer, payload+PAYLOAD_SIZE/2, PAYLOAD_SIZE/2));
		} else {
			CHECK(xrtNetBufAppendBytes(&buffer, graph->owned_bytes, 0, PAYLOAD_SIZE/2));
			CHECK(xrtNetBufAppendBytes(&buffer, graph->owned_bytes, PAYLOAD_SIZE/2, PAYLOAD_SIZE/2));
		}
		CHECK(buffer.Blocks == 2); result = xrtNetStreamSendBuffer(graph->client, &buffer);
		CHECK(result == XNET_RESULT_OK && !buffer.Head); xrtNetBufClear(&buffer);
	}
	CHECK(result == XNET_RESULT_OK); xrtFree(payload);
	xrtAtomic32Store(&graph->payload_ready, 1, XMEMORY_RELEASE);
}
static void buffered(TcpGraph* graph)
{
	if (graph->payload_mode == 4 || graph->payload_mode == 6) {
		uint8* payload = xrtMalloc(PAYLOAD_SIZE); CHECK(payload); memset(payload, 'X', PAYLOAD_SIZE);
		memcpy(payload, "A\0B", 3); graph->owned_bytes = xrtNetBytesCopy(payload, PAYLOAD_SIZE);
		xrtFree(payload); CHECK(graph->owned_bytes);
	}
	CHECK(xrtNetEnginePost(graph->engine, 0, send_payload, graph));
	XL6_DeadlineV1 deadline = xl6DeadlineAfterMillisecondsV1(5000);
	while (!xrtAtomic32Load(&graph->payload_ready, XMEMORY_ACQUIRE)) {
		CHECK(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
	}
	if (graph->payload_mode == 2) {
		uint8* payload = xrtMalloc(PAYLOAD_SIZE); CHECK(payload); memset(payload, 'X', PAYLOAD_SIZE);
		memcpy(payload, "A\0B", 3); CHECK(xrtNetStreamSend(graph->client, payload, PAYLOAD_SIZE) == XNET_RESULT_OK);
		xrtFree(payload);
	}
	xrtownershipscope freeze = {0};
	for (;;) {
		snapshot(graph, false, &freeze);
		xnetstream* peer = (xnetstream*)graph->refs[3].Data;
		if (peer->ReadBuffer.Size >= 3) {
			xnetblock* first = peer->ReadBuffer.Head; CHECK(first && first->Class < XRT_NET_BLOCK_REF);
			CHECK(memcmp(first->Data+first->Begin, "A\0B", 3) == 0);
			xnetbufpool* pool = graph->engine->Workers[0].BufferPool;
			size_t count = 0; CHECK(graph->refs[4].Ops->Count(pool, &count) && count == 1+pool->Leases);
			CHECK(pool->Leases && pool->ParentHeld);
			CHECK(graph->refs[0].Ops->Count(graph->engine, &count) && count == 5);
			bool queued = graph->client->WriteBuffer.Size > 0;
			if (queued) ++retained_write_cases;
			else CHECK(xrtNetStreamPending(graph->client) == 0);
			CHECK(graph->refs[2].Ops->Count(graph->client, &count) && count == (queued && graph->payload_mode != 1 ? 3u : 2u));
			CHECK(xrtOwnershipScopeEnd(&freeze)); break;
		}
		if (xl6DeadlineExpiredV1(deadline)) fprintf(stderr, "buffered mode=%u read=%zu write=%zu pending=%zu\n",
			graph->payload_mode, peer->ReadBuffer.Size, graph->client->WriteBuffer.Size, xrtNetStreamPending(graph->client));
		CHECK(xrtOwnershipScopeEnd(&freeze)); CHECK(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
	}
	++buffered_cases;
}
static void lifecycle(xnetportbackend backend, bool timers, bool future, bool errors, unsigned payload_mode)
{
	if (payload_mode) { printf("buffer lifecycle mode=%u backend=%d\n", payload_mode, (int)backend); fflush(stdout); }
	xmemdebugsnapshot before; xrtClearError(); xrtMemDebugSnapshot(&before);
	TcpGraph graph = {0}; graph.payload_mode = payload_mode;
	xrtAtomic32Init(&graph.payload_ready, 0); xrtAtomic32Init(&graph.timers_ready, 0); create(&graph, backend, future);
	xrtownershipscope freeze = {0}; snapshot(&graph, false, &freeze);
	graph.refs[3] = xrtNetStreamOwnership(graph.listener->AcceptHead); /* borrowed until real Hold */
	CHECK(graph.refs[3].Data);
	CHECK(xrtOwnershipScopeEnd(&freeze));
	if (payload_mode) buffered(&graph);
	if (timers) {
		CHECK(xrtNetEnginePost(graph.engine, 0, add_timers, &graph));
		XL6_DeadlineV1 deadline = xl6DeadlineAfterMillisecondsV1(5000);
		while (!xrtAtomic32Load(&graph.timers_ready, XMEMORY_ACQUIRE)) {
			CHECK(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
		}
		++timer_cases;
	}
	if (future) {
		xfuture* pending = xrtNetStreamWaitAsync(graph.client, XNET_STREAM_WAIT_CLOSE);
		CHECK(pending);
		XL6_DeadlineV1 deadline = xl6DeadlineAfterMillisecondsV1(5000);
		for (;;) {
			freeze_begin(&freeze);
			if (!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(graph.engine)) break;
			CHECK(xrtOwnershipScopeEnd(&freeze)); CHECK(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
		}
		size_t count = 123;
		CHECK(graph.refs[2].Ops->Count(graph.refs[2].Data, &count) && count == 3);
		const xrtownershippreparationv1* unchanged = (const xrtownershippreparationv1*)&graph;
		CHECK(xrtNetStreamOwnershipAdapterV1(graph.refs[2], &unchanged) && unchanged != (const xrtownershippreparationv1*)&graph);
		CHECK(!xrtFutureOwnershipAdapterV1(xrtFutureOwnership(pending), NULL, 0));
		CHECK(xrtOwnershipScopeEnd(&freeze));
		CHECK(xrtFutureCancel(pending)); xrtFutureDestroy(pending); ++future_cases;
	}
	snapshot(&graph, false, &freeze);
	if (errors) {
		/* Offline owned diagnostics, not a malformed network exchange. Transfer
		 * exactly the same real immutable Error/Cause chain as native IO errors. */
		xerror* cause = xrtErrorCreate(XERR_STATE, "tcp.ownership.fixture", 1, "owned cause"); CHECK(cause);
		xerror* error = xrtErrorWrap(cause, XERR_IO, "tcp.ownership.fixture", 2, "owned transport diagnostic"); CHECK(error);
		CHECK(!graph.client->Error); graph.client->Error = error; xrtErrorFree(cause);
		graph.refs[5] = xrtErrorOwnership(error); graph.refs[6] = xrtErrorOwnership(cause);
		graph.nodes = 7; ++error_cases;
	}
	xrtownershipresult result;
	xrtownershipref slots[4] = {graph.refs[0],graph.refs[1],graph.refs[2]};
	size_t creators = 3;
	if (graph.owned_bytes) slots[creators++] = xrtNetBytesOwnership(graph.owned_bytes);
	CHECK(xrtOwnershipInspect(graph.refs, graph.nodes, slots, creators, &result));
	/* Inspect includes the three real creator slots in EdgeCount too. */
	CHECK(result.NodeCount == graph.nodes && result.ExternalRootCount == 0);
	if (!payload_mode) CHECK(result.EdgeCount == (timers ? 13u : 11u)+(errors ? 2u : 0u));
	else CHECK(result.EdgeCount > 11);
	const xrtownershippreparationv1* unchanged = (const xrtownershippreparationv1*)&graph;
	CHECK(!xrtNetEngineOwnershipAdapterV2(graph.refs[0], NULL, 0, NULL, 0, NULL, 0, &unchanged));
	CHECK(unchanged == (const xrtownershippreparationv1*)&graph);
	xnetruntimeownershipv1 copy = *xrtNetStreamRuntimeOwnershipV1Get();
	const xnetruntimeownershipv1* wrong[] = {&copy, xrtNetListenerRuntimeOwnershipV1Get()};
	CHECK(!xrtNetEngineOwnershipAdapterV3(graph.refs[0], NULL, 0, NULL, 0, NULL, 0, wrong, 2, &unchanged));
	CHECK(unchanged == (const xrtownershippreparationv1*)&graph); ++refusal_cases;
	const xnetruntimeownershipv1* runtimes[] = {xrtNetStreamRuntimeOwnershipV1Get(),xrtNetListenerRuntimeOwnershipV1Get()};
	graph.adapters[0] = xrtNetEngineOwnershipAdapterV3(graph.refs[0], NULL, 0, NULL, 0, NULL, 0, runtimes, 2, &graph.preparations[0]);
	graph.adapters[1] = xrtNetListenerOwnershipAdapterV1(graph.refs[1], &graph.preparations[1]);
	for (size_t i = 2; i < 4; ++i) graph.adapters[i] = xrtNetStreamOwnershipAdapterV1(graph.refs[i], &graph.preparations[i]);
	graph.adapters[4] = xrtNetBufPoolOwnershipAdapterV1(graph.refs[4], &graph.preparations[4]);
	for (size_t i = 5; i < graph.nodes; ++i) {
		graph.adapters[i] = xrtNetBytesOwnershipAdapterV1(graph.refs[i]);
		if (!graph.adapters[i]) graph.adapters[i] = xrtErrorOwnershipAdapterV1(graph.refs[i]);
	}
	for (size_t i = 0; i < graph.nodes; ++i) {
		CHECK(graph.adapters[i] && (i >= 5 || graph.preparations[i]));
		CHECK(graph.adapters[i]->Hold(graph.refs[i].Data));
		CHECK(graph.adapters[i]->Claim(graph.refs[i].Data, &graph));
		if (i < 5) CHECK(!graph.adapters[i]->Claim(graph.refs[i].Data, &result));
		graph.adapters[i]->Restore(graph.refs[i].Data, &graph);
		CHECK(graph.adapters[i]->Claim(graph.refs[i].Data, &graph));
	}
	CHECK(xrtOwnershipScopeEnd(&freeze));
	/* New weak/raw publication is quarantined; no packet is sent. */
	CHECK(!xrtNetListenerAccept(graph.listener)); xrtClearError();
	CHECK(!xrtNetStreamWaitAsync(graph.client, XNET_STREAM_WAIT_CLOSE)); xrtClearError();
	CHECK(graph.preparations[0]->Prepare(graph.engine, &graph) == XRT_OWNERSHIP_PREPARE_BUSY);
	XL6_DeadlineV1 deadline = xl6DeadlineAfterMillisecondsV1(5000);
	bool ready[5] = {false,false,false,false,false};
	for (;;) {
		bool all = true;
		for (size_t step = 0; step < 5; ++step) {
			size_t i = (step+1)%5; /* actual children before Engine */
			if (ready[i]) continue;
			snapshot(&graph, true, &freeze); CHECK(xrtOwnershipScopeEnd(&freeze));
			xrtownershipprepareresult state = graph.preparations[i]->Prepare(graph.refs[i].Data, &graph);
			CHECK(state != XRT_OWNERSHIP_PREPARE_FAILED); ready[i] = state == XRT_OWNERSHIP_PREPARE_READY;
			all = all && ready[i];
		}
		if (all) break;
		CHECK(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
	}
	snapshot(&graph, true, &freeze);
	for (size_t i = 0; i < graph.nodes; ++i) {
		if (i < 5) CHECK(graph.preparations[i]->Ready(graph.refs[i].Data));
		graph.adapters[i]->Clear(graph.refs[i].Data, &graph);
	}
	CHECK(xrtOwnershipScopeEnd(&freeze));
	CHECK(graph.adapters[0]->Finish(graph.engine, &graph));
	CHECK(graph.adapters[0]->Finish(graph.engine, &graph));
	CHECK(xrtNetEngineTryDestroy(graph.engine) == XNET_RETIRE_READY);
	xrtNetListenerDestroy(graph.listener); xrtNetStreamDestroy(graph.client);
	xrtNetBytesDestroy(graph.owned_bytes);
	for (size_t i = 1; i < graph.nodes; ++i) graph.adapters[i]->Drop(graph.refs[i].Data);
	graph.adapters[0]->Drop(graph.engine); balanced(&before); ++graph_cases;
}
static void construction(xnetportbackend backend)
{
	for (unsigned budget = 0; budget < 8; ++budget) {
		xmemdebugsnapshot before; xrtClearError(); xrtMemDebugSnapshot(&before);
		xnetengineconfig config; xrtNetEngineConfigInit(&config);
		config.Backend = backend; config.Workers = 1; config.NodeCacheBytes = 0;
		xnetengine* engine = xrtNetEngineCreate(&config); CHECK(engine && xrtNetEngineStart(engine));
		xnetlistenconfig listen; xrtNetListenConfigInit(&listen);
		listen.Stream.ReadMode = XNET_STREAM_READ_PROBE;
		CHECK(xrtNetAddrLoopback(&listen.Address, XNET_FAMILY_IPV4, 0));
		xrtownershipscope freeze = {0}; XL6_DeadlineV1 deadline = xl6DeadlineAfterMillisecondsV1(5000);
		for (;;) {
			freeze_begin(&freeze);
			if (!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(engine)) break;
			CHECK(xrtOwnershipScopeEnd(&freeze)); CHECK(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
		}
		xnetlistener* listener = xrtNetListen(engine, &listen, NULL, NULL, NULL); CHECK(listener);
		/* A real native startup task, not an inferred callback identity, makes
		 * graph admission BUSY before the frozen worker can execute it. */
		CHECK(xrtNetEngineOwnershipQuiescenceV1Get()->Busy(engine));
		xrtownershipref ref = xrtNetEngineOwnership(engine); size_t count = 123;
		CHECK(!ref.Ops->Count(ref.Data, &count) && count == 123);
		CHECK(xrtOwnershipScopeEnd(&freeze));
		deadline = xl6DeadlineAfterMillisecondsV1(5000);
		for (;;) {
			freeze_begin(&freeze);
			if (!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(engine)) break;
			CHECK(xrtOwnershipScopeEnd(&freeze)); CHECK(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
		}
		xnetaddr address; CHECK(xrtNetListenerLocal(listener, &address));
		CHECK(xrtMemDebugFailAfter(budget)); ++allocation_budgets;
		xnetstream* client = xrtNetStreamConnect(engine, &address, 0, &listen.Stream, NULL, NULL);
		bool failed = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
		if (!client) {
			CHECK(failed); ++allocation_failures;
			CHECK(engine->RuntimeOwnerCount == 1 && engine->RuntimeOwners == &listener->RuntimeOwner);
		} else {
			CHECK(!failed && engine->RuntimeOwnerCount == 2);
			CHECK(xrtNetEngineOwnershipQuiescenceV1Get()->Busy(engine));
		}
		xrtClearError(); CHECK(xrtOwnershipScopeEnd(&freeze));
		if (client) { CHECK(xrtNetStreamAbort(client)); xrtNetStreamDestroy(client); }
		CHECK(xrtNetListenerClose(listener)); xrtNetListenerDestroy(listener);
		deadline = xl6DeadlineAfterMillisecondsV1(5000);
		for (;;) {
			xnetretireresult result = xrtNetEngineTryDestroy(engine);
			if (result == XNET_RETIRE_READY) break;
			CHECK(result == XNET_RETIRE_BUSY && !xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
		}
		balanced(&before);
	}
}
int main(int argc, char** argv)
{
	xnetportbackend backend = XNET_PORT_SELECT;
	if (argc > 1 && strcmp(argv[1], "native") == 0) {
		#if defined(_WIN32) || defined(_WIN64)
			backend = XNET_PORT_IOCP;
		#elif defined(__linux__)
			backend = XNET_PORT_EPOLL;
		#endif
	}
	CHECK(xrtMemDebugEnable(true));
	for (unsigned i = 0; i < 3; ++i) {
		lifecycle(backend, false, false, false, 0); lifecycle(backend, true, false, false, 0);
		lifecycle(backend, false, true, false, 0); lifecycle(backend, false, false, true, 0);
	}
	for (unsigned mode = 1; mode <= 6; ++mode) lifecycle(backend, false, false, false, mode);
	/* Linux must exercise every actual retained frame/batch, including the
	 * shared-bytes aggregate. Windows loopback may acknowledge every payload
	 * into kernel storage immediately; that is reported, not counted as a
	 * retained-write proof or turned into a fabricated native queue. */
	#if defined(__linux__)
		CHECK(retained_write_cases == 6);
	#endif
	construction(backend); CHECK(allocation_failures > 0 && allocation_failures < allocation_budgets);
	testMemoryDebugDrain("TCP ownership allocations did not converge");
	printf("[PASS] TCP graph backend=%d graphs=%u native-timers=%u future-reentry=%u error-DAG=%u buffered=%u retained-writes=%u policy-refusals=%u allocation-budgets=%u failures=%u\n",
		(int)backend, graph_cases, timer_cases, future_cases, error_cases, buffered_cases, retained_write_cases, refusal_cases, allocation_budgets, allocation_failures);
	return 0;
}
