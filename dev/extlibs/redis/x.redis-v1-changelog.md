# x.redis V1 Changelog

## 0.1.0

Status: [X] implemented

### Package

- Added source package at `demo5/release/libs/x.redis`.
- Added module metadata for `x.redis@0.1.0#abi1`.
- Added README and examples under `demo5/release/examples/redis`.

### RESP

- Added RESP2 command encoding helpers.
- Added RESP2 parser and common RESP3 parser support.
- Added normalized response JSON helpers.
- Added typed response extractors for simple, error, integer, bulk, array, set,
  map, bool, double, bignumber, blob error, verbatim, and nil responses.

### Client

- Added `Client` lifecycle, close, last error, timeout, and fixed idle pool.
- Added raw command helpers, JSON command builders, and pipeline helpers.
- Added synchronous pub/sub helpers.
- Added explicit `HELLO` and `HELLO AUTH` helpers.

### Redis Commands

- Added connection/server wrappers.
- Added string, hash, list, and set wrappers.
- Added fallback helpers for nil bulk reads.
- Added TTL, counter, append, strlen, hlen, hstrlen, and same-slot multi-key
  wrappers where appropriate.

### Sentinel

- Added Sentinel discovery helpers.
- Added explicit current-master connect and raw command helpers.
- Added bounded current-master retry helpers.
- Added password and username/password auth raw command helpers.
- Added explicit current-master connection/server wrappers.
- Added explicit current-master string, hash, list, and set wrappers.
- Added capability reporting through `sentinel_capabilities_json`.

### Cluster

- Added Cluster discovery helpers.
- Added `CLUSTER SLOTS` endpoint extraction and explicit slot connect.
- Added `MOVED`/`ASK` parsing, redirect connect, and `ASKING`.
- Added one-shot and bounded redirect raw command helpers.
- Added per-call key routing through fresh `CLUSTER SLOTS`.
- Added caller-managed `ClusterSlotsCache`.
- Added cached and cache-refresh-on-use key routing.
- Added password and username/password auth raw command helpers for slot owner,
  redirect target, cached slots, and cache-refresh routes.
- Added explicit cluster string, hash, list, and set wrappers.
- Added same-slot `MGET` and `MSET` wrappers.
- Added retryable cluster error classification.
- Added capability reporting through `cluster_capabilities_json`.

### Validation

- Added offline codec regression.
- Added fake Redis smoke server.
- Added opt-in real Redis smoke scripts.
- Added all-checks scripts for build, offline run, default regression selector,
  export metadata parity, and fake smoke.
- Added package export metadata parity checker.

### Handoff

- Added compact V1 status document.
- Added blocked handoff spec for binary-safe transport, TLS socket transport,
  Sentinel background monitoring, and transparent Cluster routing.
