# x.redis V1 Status

Status: [X] source package implemented and default all-checks verified

Live package:

```text
demo5/release/libs/x.redis/
```

Package identity:

```text
x.redis@0.1.0#abi1
```

## Implemented

- [X] RESP2 command encoding.
- [X] RESP2 and common RESP3 response parsing.
- [X] Normalized response JSON and typed response extractors.
- [X] Text-safe Redis client over `std.net.tcp`.
- [X] Raw commands, JSON command builders, and pipeline helpers.
- [X] Connection lifecycle and fixed-slot idle pool.
- [X] String, hash, list, set, connection, and server wrappers.
- [X] Basic synchronous pub/sub helpers.
- [X] Explicit RESP `HELLO` helpers.
- [X] Sentinel discovery, explicit current-master command helpers, auth raw
  helpers, and current-master wrappers.
- [X] Cluster discovery, redirect parsing, ASKING, slots cache, explicit key
  routing, auth raw helpers, and cache-refresh wrappers.
- [X] Capability helpers:
  `transport_limits_json`, `sentinel_capabilities_json`, and
  `cluster_capabilities_json`.

## Default Validation

Run from the repository root:

```bat
dev\extlibs\redis\run_x_redis_all_checks.bat
```

```sh
dev/extlibs/redis/run_x_redis_all_checks.sh
```

The default validation does not require a real Redis instance. It runs:

- [X] `redis_basic.xl` build.
- [X] source/package export metadata parity check.
- [X] `redis_offline_codec.xl` offline run.
- [X] `xl5_test.exe runtime.redis_offline_codec`.
- [X] fake Redis smoke.

## Remaining Blocked Work

- [!] Native binary-safe transport.
- [!] TLS socket transport.
- [!] Sentinel background monitoring and failover-aware persistent connections.
- [!] Transparent Cluster routing for ordinary commands.

The concrete handoff contract for these items is tracked in:

```text
dev/extlibs/redis/x.redis-blocked-handoff-spec.md
```

## Intentional V1 Limits

- [!] `std.net.tcp` is string-oriented; V1 is not binary-safe.
- [!] `std.net.tls` has no TLS socket I/O API for Redis transport.
- [!] `std.net.tcp.connect` does not expose connect timeout control.
- [!] Cluster/Sentinel automation is explicit, not transparent.
