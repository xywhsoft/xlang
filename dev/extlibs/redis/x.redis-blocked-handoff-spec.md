# x.redis Blocked Handoff Spec

Status: handoff spec

This document turns the remaining blocked `x.redis` work into concrete
implementation contracts for a future runtime transport layer or a separately
maintained Redis transport library.

## Status Legend

- [ ] not started
- [X] complete
- [~] in progress
- [!] blocked or needs decision

## 1. Native Binary-Safe Transport

Status: [!]

Current blocker:

- `std.net.tcp.send(point socket, string text)` is length-by-`strlen`.
- `std.net.tcp.recv(point socket, int size, int timeout_ms)` returns `string`.
- Redis bulk strings can contain `NUL` bytes and arbitrary byte sequences.

Required transport contract:

- [ ] Provide a byte buffer type or byte-array API that preserves explicit
  length and does not stop at `NUL`.
- [ ] Provide `connect(host, port, timeout_ms)` with connect timeout behavior.
- [ ] Provide `send_all(socket, bytes, length)` or equivalent.
- [ ] Provide `recv_some(socket, max_bytes, timeout_ms)` returning bytes plus
  actual byte count.
- [ ] Provide deterministic `close(socket)`.
- [ ] Preserve existing `x.redis` source API compatibility where practical.

Acceptance:

- [ ] Can send and receive a Redis bulk string containing `NUL`.
- [ ] Can parse RESP frames split across arbitrary byte boundaries.
- [ ] Can reject oversized responses using the existing max response limit.
- [ ] Existing text-safe examples still pass unchanged.
- [ ] `transport_binary_safe()` can return `true` for the new transport.

## 2. TLS Socket Transport

Status: [!]

Current blocker:

- `std.net.tls` currently exposes TLS metadata/config helpers.
- It does not expose TLS socket `connect/send/recv/close`.

Required transport contract:

- [ ] Provide TLS client connect with host, port, SNI, verification flag, and
  timeout.
- [ ] Provide TLS send/recv using the same binary-safe byte contract as native
  transport.
- [ ] Surface certificate verification failure as a readable error.
- [ ] Preserve explicit close behavior.
- [ ] Allow Redis AUTH/HELLO to run after TLS connect.

Acceptance:

- [ ] Can connect to a Redis TLS endpoint with certificate verification enabled.
- [ ] Can connect with verification disabled for local disposable smoke only.
- [ ] Can run `PING`, `AUTH`, `SET`, and `GET` over TLS.
- [ ] Can parse an invalid certificate failure without crashing.
- [ ] `transport_tls()` can return `true` for the new transport.

## 3. Sentinel Background Monitoring

Status: [!]

Current blocker:

- V1 has no background task/event-loop ownership model.
- Current Sentinel helpers intentionally use explicit lookup and temporary
  current-master command execution.

Required client contract:

- [ ] Define who owns background polling/subscription execution.
- [ ] Track Sentinel addresses, master name, current master endpoint, and last
  refresh error.
- [ ] Refresh master endpoint after `+switch-master` or bounded polling.
- [ ] Decide whether existing checked-out clients are invalidated or allowed to
  drain after failover.
- [ ] Define authentication settings for both Sentinel and Redis master
  connections.

Acceptance:

- [ ] Detects master endpoint changes without caller issuing an explicit lookup.
- [ ] New commands after failover use the new master endpoint.
- [ ] Old sockets are closed or invalidated according to a documented policy.
- [ ] Authenticated Sentinel and authenticated Redis master flows are covered.
- [ ] `sentinel_capabilities_json()` can report background monitoring support.

## 4. Transparent Cluster Routing

Status: [!]

Current blocker:

- V1 deliberately exposes explicit key/slot/cache routing.
- Transparent routing for ordinary `command*` calls requires command-key
  extraction and cross-slot policy.

Required routing contract:

- [ ] Define command metadata for key positions, multi-key rules, and read/write
  behavior.
- [ ] Define cross-slot behavior for multi-key commands.
- [ ] Define whether ordinary `command*` stays single-node or becomes
  cluster-aware through a new client type.
- [ ] Maintain and refresh a slots cache internally.
- [ ] Follow `MOVED` and `ASK`, including `ASKING`.
- [ ] Refresh on retryable errors such as `READONLY`, `TRYAGAIN`, and
  `CLUSTERDOWN`.
- [ ] Support authentication on temporary and cached node connections.

Acceptance:

- [ ] `GET` and `SET` on different slots route to correct nodes without caller
  selecting a node.
- [ ] `MOVED` and `ASK` are followed automatically.
- [ ] Stale slots are refreshed automatically.
- [ ] Same-slot multi-key commands work when hash tags are used.
- [ ] Cross-slot multi-key commands return a documented error.
- [ ] `cluster_capabilities_json()` can report transparent routing support.
