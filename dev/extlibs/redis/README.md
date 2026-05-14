# x.redis extension design

Status: V1 source package implemented and tracked
Implementation target: `demo5/release/libs/x.redis`

This directory records the V1 design, implementation tracker, validation entry
points, and blocked-work handoff specs for the pure xlang Redis extension. V1
uses `std.net.tcp` and implements the RESP client protocol in xlang source code.
Native C transport work and third-party C client integrations are out of scope
for `x.redis` and should be maintained as separate libraries. The current source
package exposes transport capability helpers so callers can detect that V1 is
not binary-safe and does not provide Redis-over-TLS sockets.

## Documents

- [x.redis-v1-design.md](x.redis-v1-design.md): package boundary, API, RESP
  codec design, implementation details, examples, validation rules, and risks.
- [x.redis-implementation-tracker.md](x.redis-implementation-tracker.md):
  phased implementation tracker using `[ ]`, `[X]`, `[~]`, and `[!]`.
- [x.redis-v1-status.md](x.redis-v1-status.md): compact current V1 status,
  validation entry points, capability boundaries, and remaining blocked work.
- [x.redis-v1-changelog.md](x.redis-v1-changelog.md): summary of implemented
  V1 package, API, validation, and handoff changes.
- [x.redis-blocked-handoff-spec.md](x.redis-blocked-handoff-spec.md): concrete
  contracts and acceptance criteria for the remaining `[!]` transport,
  Sentinel automation, and transparent Cluster routing work.

## Implementation Rule

`dev/extlibs/redis` is only the design and tracking area. Do not place the live
package implementation here.

The live V1 package should be created as:

```text
demo5/release/libs/x.redis/
  package.json
  README.md
  src/redis.xl
```

Examples and smoke tests should be added under:

```text
demo5/release/examples/redis/
```

Optional real Redis smoke scripts should live under this design directory:

```text
dev/extlibs/redis/run_x_redis_smoke.bat
dev/extlibs/redis/run_x_redis_smoke.sh
```

Default Redis extension validation that does not require a real Redis instance:

```text
dev/extlibs/redis/run_x_redis_all_checks.bat
dev/extlibs/redis/run_x_redis_all_checks.sh
```

The all-checks scripts also verify that `src/redis.xl` exports match
`package.json` metadata through `check_x_redis_exports.js`.

Default regression must not depend on a running Redis server. Real smoke tests
must be opt-in and should read connection settings from environment variables.
