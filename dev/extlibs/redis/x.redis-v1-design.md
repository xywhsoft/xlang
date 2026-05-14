# x.redis V1 Design

Status: implemented and tracked
Implementation target: `demo5/release/libs/x.redis`
Strategy: pure xlang package using `std.net.tcp`

## 1. Goal

`x.redis` provides a small, practical Redis client for xlang. V1 should be good
enough for scripts, local tools, cache access, and smoke automation without
adding third-party native dependencies.

V1 must:

- use the existing `std.net.tcp` module for network transport;
- implement Redis RESP2 request encoding and RESP2/common RESP3 response parsing
  in xlang;
- expose a stable high-level API for common commands;
- provide a raw command escape hatch for commands not wrapped yet;
- keep default tests offline and deterministic;
- document the transport limits that come from current `std.net.tcp.recv`.

## 2. Non-Goals

- No third-party C client dependency in `x.redis`; those integrations are
  maintained as separate libraries.
- No custom native socket layer in V1.
- No transparent Redis Cluster routing, Sentinel background monitoring, or TLS
  sockets in V1. Explicit Cluster and Sentinel helpers are available.
- No automatic RESP3 command negotiation in V1; callers can opt in through
  explicit `HELLO` helpers.
- No pub/sub event loop in V1.
- No binary-safe bulk string guarantee in V1 because current TCP receive returns
  xlang `string` and uses text-oriented boundaries.
- No dynamic connection pool in V1; a fixed-slot idle pool is available.

These can be added later without breaking the high-level API if the client class
stays as the only public connection handle.

## 3. Package Layout

Live package:

```text
demo5/release/libs/x.redis/
  package.json
  README.md
  src/redis.xl
```

Example package metadata:

```json
{
  "format": "xle-1",
  "package": {
    "name": "x.redis",
    "version": "0.1.0",
    "abiVersion": "1",
    "xlangAbi": "1",
    "abiId": "x.redis@0.1.0#abi1"
  },
  "artifact": {
    "kind": "source",
    "path": "src\\redis.xl"
  },
  "dependencies": [],
  "modules": [
    {
      "key": "x.redis",
      "abiId": "x.redis@0.1.0#abi1::x.redis",
      "file": "src\\redis.xl",
      "root": 0,
      "native": 0,
      "binding": "",
      "imports": ["std.net.tcp"],
      "exports": []
    }
  ],
  "functions": []
}
```

Examples:

```text
demo5/release/examples/redis/redis_basic.xl
demo5/release/examples/redis/redis_offline_codec.xl
```

Optional real smoke:

```text
dev/extlibs/redis/run_x_redis_smoke.bat
dev/extlibs/redis/run_x_redis_smoke.sh
```

Default validation without a real Redis instance:

```text
dev/extlibs/redis/run_x_redis_all_checks.bat
dev/extlibs/redis/run_x_redis_all_checks.sh
```

## 4. Import and Naming

The public package name should be `x.redis`.

Usage:

```xl
import x.redis as redis
```

Reasoning:

- The `x.*` prefix marks this as an official extension library rather than a
  bundled core `std.*` module.
- It avoids confusion with a possible future third-party package named `redis`.
- It follows the official extension naming already used by packages such as
  `x.uia`, `x.webhook`, and `x.volc.*`.

## 5. Public API

### 5.1 Constants

```xl
export const int OK = 0
export const int ERR = 1
export const int TIMEOUT = 2
export const int CLOSED = 3
export const int PROTOCOL_ERROR = 4
```

### 5.2 Client Class

```xl
class Client extends object
  nocopy
  var socket as point
  var host as string
  var port as int
  var timeout_ms as int
  var last_error_text as string

  create(point socket, string host, int port, int timeout_ms)
  destroy()

  func valid() as bool
  func close() as bool
  func last_error() as string
  func set_timeout(int timeout_ms) as void
end
```

Lifecycle rules:

- `connect` returns a `Client`.
- `Client.destroy()` closes the TCP socket if it is still open.
- `close()` is idempotent: a second close returns `false` or `true` consistently
  as documented, but must not crash.
- Every public command checks `valid()` and records a readable `last_error`.

### 5.3 Connection API

```xl
func connect(string host, int port) as Client
func connect(string host, int port, int timeout_ms) as Client
func close(Client client) as bool
func last_error(Client client) as string
```

Pool API:

```xl
func pool(string host, int port, int max_idle) as Pool
func pool(string host, int port, int timeout_ms, int max_idle) as Pool
func pool_acquire(Pool pool_value) as Client
func pool_release(Pool pool_value, Client client) as bool
func pool_close(Pool pool_value) as int
func pool_idle_count(Pool pool_value) as int
```

`pool_release` takes ownership of the client. Call it as
`pool_release(pool_value, move(client))`. V1 uses a fixed four-slot idle socket
pool to stay compatible with current `nocopy` container runtime support.

Default timeout:

- `connect(host, port)` uses `5000` ms for receive operations.
- Current `std.net.tcp.connect` has no explicit connect timeout. Document this
  as a V1 transport limitation.

### 5.4 Raw Command API

```xl
func command_json(Client client, string argv_json) as string
func command_array(Client client, list<string> argv) as string
func command_array(Client client, array<string> argv) as string
func command1(Client client, string a0) as string
func command2(Client client, string a0, string a1) as string
func command3(Client client, string a0, string a1, string a2) as string
func command4(Client client, string a0, string a1, string a2, string a3) as string
func command5(Client client, string a0, string a1, string a2, string a3, string a4) as string
func argv_json1(string a0) as string
func argv_json2(string a0, string a1) as string
func argv_json3(string a0, string a1, string a2) as string
func argv_json4(string a0, string a1, string a2, string a3) as string
func argv_json_list(string command, array<string> values) as string
func pipeline_json_array(array<string> commands) as string
func pipeline_json1(string command0) as string
func pipeline_json2(string command0, string command1) as string
func pipeline_json3(string command0, string command1, string command2) as string
func pipeline_json4(string command0, string command1, string command2, string command3) as string
func pipeline_json5(string command0, string command1, string command2, string command3, string command4) as string
func transport_name() as string
func transport_binary_safe() as bool
func transport_tls() as bool
func transport_limits_json() as string
func sentinel_capabilities_json() as string
func cluster_capabilities_json() as string
func response_type(string response_json) as string
func response_value(string response_json) as string
func response_int(string response_json) as int
func response_bool(string response_json) as bool
func response_double_text(string response_json) as string
func response_bignumber_text(string response_json) as string
func response_blob_error_text(string response_json) as string
func response_verbatim_text(string response_json) as string
func response_nil(string response_json) as bool
func response_ok(string response_json) as bool
func response_error(string response_json) as bool
func response_error_text(string response_json) as string
func response_array_value(string response_json, int index) as string
func response_array_length(string response_json) as int
func response_set_value(string response_json, int index) as string
func response_set_length(string response_json) as int
func response_map_length(string response_json) as int
func response_map_key_value(string response_json, int index) as string
func response_map_value_value(string response_json, int index) as string
func response_map_value(string response_json, string key) as string
func response_map_has_key(string response_json, string key) as bool
func cluster_command_redirect_once_json(Client client, string argv_json) as string
func cluster_command_redirect_once_json(Client client, string argv_json, int timeout_ms) as string
func cluster_command_redirect_retry_json(Client client, string argv_json, int max_redirects) as string
func cluster_command_redirect_retry_json(Client client, string argv_json, int max_redirects, int timeout_ms) as string
func cluster_retryable_error(string response_json) as bool
func cluster_command_slot_once_json(Client seed, string slots_response_json, string key, string argv_json) as string
func cluster_command_slot_once_json(Client seed, string slots_response_json, string key, string argv_json, int timeout_ms) as string
func cluster_command_slot_retry_json(Client seed, string slots_response_json, string key, string argv_json, int max_redirects) as string
func cluster_command_slot_retry_json(Client seed, string slots_response_json, string key, string argv_json, int max_redirects, int timeout_ms) as string
func cluster_command_key_once_json(Client seed, string key, string argv_json) as string
func cluster_command_key_once_json(Client seed, string key, string argv_json, int timeout_ms) as string
func cluster_command_key_retry_json(Client seed, string key, string argv_json, int max_redirects) as string
func cluster_command_key_retry_json(Client seed, string key, string argv_json, int max_redirects, int timeout_ms) as string
func cluster_command_slot_once_password_json(Client seed, string slots_response_json, string key, string password, string argv_json) as string
func cluster_command_slot_once_password_json(Client seed, string slots_response_json, string key, string password, string argv_json, int timeout_ms) as string
func cluster_command_slot_retry_password_json(Client seed, string slots_response_json, string key, string password, string argv_json, int max_redirects) as string
func cluster_command_slot_retry_password_json(Client seed, string slots_response_json, string key, string password, string argv_json, int max_redirects, int timeout_ms) as string
func cluster_command_slot_once_auth_json(Client seed, string slots_response_json, string key, string username, string password, string argv_json) as string
func cluster_command_slot_once_auth_json(Client seed, string slots_response_json, string key, string username, string password, string argv_json, int timeout_ms) as string
func cluster_command_slot_retry_auth_json(Client seed, string slots_response_json, string key, string username, string password, string argv_json, int max_redirects) as string
func cluster_command_slot_retry_auth_json(Client seed, string slots_response_json, string key, string username, string password, string argv_json, int max_redirects, int timeout_ms) as string
func cluster_command_key_once_password_json(Client seed, string key, string password, string argv_json) as string
func cluster_command_key_once_password_json(Client seed, string key, string password, string argv_json, int timeout_ms) as string
func cluster_command_key_retry_password_json(Client seed, string key, string password, string argv_json, int max_redirects) as string
func cluster_command_key_retry_password_json(Client seed, string key, string password, string argv_json, int max_redirects, int timeout_ms) as string
func cluster_command_key_once_auth_json(Client seed, string key, string username, string password, string argv_json) as string
func cluster_command_key_once_auth_json(Client seed, string key, string username, string password, string argv_json, int timeout_ms) as string
func cluster_command_key_retry_auth_json(Client seed, string key, string username, string password, string argv_json, int max_redirects) as string
func cluster_command_key_retry_auth_json(Client seed, string key, string username, string password, string argv_json, int max_redirects, int timeout_ms) as string
func cluster_slots_cache(string slots_response_json) as ClusterSlotsCache
func cluster_slots_cache_json(ClusterSlotsCache cache) as string
func cluster_slots_cache_valid(ClusterSlotsCache cache) as bool
func cluster_slots_cache_refresh(ClusterSlotsCache cache, Client seed) as bool
func cluster_command_cached_key_once_json(Client seed, ClusterSlotsCache cache, string key, string argv_json) as string
func cluster_command_cached_key_once_json(Client seed, ClusterSlotsCache cache, string key, string argv_json, int timeout_ms) as string
func cluster_command_cached_key_retry_json(Client seed, ClusterSlotsCache cache, string key, string argv_json, int max_redirects) as string
func cluster_command_cached_key_retry_json(Client seed, ClusterSlotsCache cache, string key, string argv_json, int max_redirects, int timeout_ms) as string
func cluster_command_cache_refresh_key_once_json(Client seed, ClusterSlotsCache cache, string key, string argv_json) as string
func cluster_command_cache_refresh_key_once_json(Client seed, ClusterSlotsCache cache, string key, string argv_json, int timeout_ms) as string
func cluster_command_cache_refresh_key_retry_json(Client seed, ClusterSlotsCache cache, string key, string argv_json, int max_redirects) as string
func cluster_command_cache_refresh_key_retry_json(Client seed, ClusterSlotsCache cache, string key, string argv_json, int max_redirects, int timeout_ms) as string
func cluster_command_cached_key_once_password_json(Client seed, ClusterSlotsCache cache, string key, string password, string argv_json) as string
func cluster_command_cached_key_once_password_json(Client seed, ClusterSlotsCache cache, string key, string password, string argv_json, int timeout_ms) as string
func cluster_command_cached_key_retry_password_json(Client seed, ClusterSlotsCache cache, string key, string password, string argv_json, int max_redirects) as string
func cluster_command_cached_key_retry_password_json(Client seed, ClusterSlotsCache cache, string key, string password, string argv_json, int max_redirects, int timeout_ms) as string
func cluster_command_cached_key_once_auth_json(Client seed, ClusterSlotsCache cache, string key, string username, string password, string argv_json) as string
func cluster_command_cached_key_once_auth_json(Client seed, ClusterSlotsCache cache, string key, string username, string password, string argv_json, int timeout_ms) as string
func cluster_command_cached_key_retry_auth_json(Client seed, ClusterSlotsCache cache, string key, string username, string password, string argv_json, int max_redirects) as string
func cluster_command_cached_key_retry_auth_json(Client seed, ClusterSlotsCache cache, string key, string username, string password, string argv_json, int max_redirects, int timeout_ms) as string
func cluster_command_cache_refresh_key_once_password_json(Client seed, ClusterSlotsCache cache, string key, string password, string argv_json) as string
func cluster_command_cache_refresh_key_once_password_json(Client seed, ClusterSlotsCache cache, string key, string password, string argv_json, int timeout_ms) as string
func cluster_command_cache_refresh_key_retry_password_json(Client seed, ClusterSlotsCache cache, string key, string password, string argv_json, int max_redirects) as string
func cluster_command_cache_refresh_key_retry_password_json(Client seed, ClusterSlotsCache cache, string key, string password, string argv_json, int max_redirects, int timeout_ms) as string
func cluster_command_cache_refresh_key_once_auth_json(Client seed, ClusterSlotsCache cache, string key, string username, string password, string argv_json) as string
func cluster_command_cache_refresh_key_once_auth_json(Client seed, ClusterSlotsCache cache, string key, string username, string password, string argv_json, int timeout_ms) as string
func cluster_command_cache_refresh_key_retry_auth_json(Client seed, ClusterSlotsCache cache, string key, string username, string password, string argv_json, int max_redirects) as string
func cluster_command_cache_refresh_key_retry_auth_json(Client seed, ClusterSlotsCache cache, string key, string username, string password, string argv_json, int max_redirects, int timeout_ms) as string
func cluster_get(Client seed, ClusterSlotsCache cache, string key) as string
func cluster_get_or(Client seed, ClusterSlotsCache cache, string key, string fallback) as string
func cluster_set(Client seed, ClusterSlotsCache cache, string key, string value) as bool
func cluster_set_nx(Client seed, ClusterSlotsCache cache, string key, string value) as bool
func cluster_set_xx(Client seed, ClusterSlotsCache cache, string key, string value) as bool
func cluster_set_ex(Client seed, ClusterSlotsCache cache, string key, string value, int seconds) as bool
func cluster_mget_json(Client seed, ClusterSlotsCache cache, array<string> keys) as string
func cluster_mset(Client seed, ClusterSlotsCache cache, array<string> key_values) as bool
func cluster_append(Client seed, ClusterSlotsCache cache, string key, string value) as int
func cluster_strlen(Client seed, ClusterSlotsCache cache, string key) as int
func cluster_del(Client seed, ClusterSlotsCache cache, string key) as int
func cluster_exists(Client seed, ClusterSlotsCache cache, string key) as bool
func cluster_expire(Client seed, ClusterSlotsCache cache, string key, int seconds) as bool
func cluster_persist(Client seed, ClusterSlotsCache cache, string key) as bool
func cluster_ttl(Client seed, ClusterSlotsCache cache, string key) as int
func cluster_pttl(Client seed, ClusterSlotsCache cache, string key) as int
func cluster_incr(Client seed, ClusterSlotsCache cache, string key) as int
func cluster_incr_by(Client seed, ClusterSlotsCache cache, string key, int amount) as int
func cluster_decr(Client seed, ClusterSlotsCache cache, string key) as int
func cluster_decr_by(Client seed, ClusterSlotsCache cache, string key, int amount) as int
func cluster_hget(Client seed, ClusterSlotsCache cache, string key, string field) as string
func cluster_hget_or(Client seed, ClusterSlotsCache cache, string key, string field, string fallback) as string
func cluster_hset(Client seed, ClusterSlotsCache cache, string key, string field, string value) as int
func cluster_hdel(Client seed, ClusterSlotsCache cache, string key, string field) as int
func cluster_hexists(Client seed, ClusterSlotsCache cache, string key, string field) as bool
func cluster_hlen(Client seed, ClusterSlotsCache cache, string key) as int
func cluster_hstrlen(Client seed, ClusterSlotsCache cache, string key, string field) as int
func cluster_hgetall_json(Client seed, ClusterSlotsCache cache, string key) as string
func cluster_lpush(Client seed, ClusterSlotsCache cache, string key, string value) as int
func cluster_rpush(Client seed, ClusterSlotsCache cache, string key, string value) as int
func cluster_lpop(Client seed, ClusterSlotsCache cache, string key) as string
func cluster_lpop_or(Client seed, ClusterSlotsCache cache, string key, string fallback) as string
func cluster_rpop(Client seed, ClusterSlotsCache cache, string key) as string
func cluster_rpop_or(Client seed, ClusterSlotsCache cache, string key, string fallback) as string
func cluster_llen(Client seed, ClusterSlotsCache cache, string key) as int
func cluster_lrange_json(Client seed, ClusterSlotsCache cache, string key, int start, int stop) as string
func cluster_sadd(Client seed, ClusterSlotsCache cache, string key, string member) as int
func cluster_srem(Client seed, ClusterSlotsCache cache, string key, string member) as int
func cluster_sismember(Client seed, ClusterSlotsCache cache, string key, string member) as bool
func cluster_smembers_json(Client seed, ClusterSlotsCache cache, string key) as string
func command_pipeline_json(Client client, string commands_json) as string
func read_response_json(Client client) as string
```

`command_json` returns a normalized JSON response:

```json
{"type":"simple","value":"OK"}
{"type":"error","value":"ERR unknown command"}
{"type":"integer","value":1}
{"type":"bulk","value":"hello","nil":false}
{"type":"bulk","value":"","nil":true}
{"type":"array","items":[{"type":"bulk","value":"a","nil":false}]}
```

The fixed-arity helpers keep common wrapper implementation simple; `command_array`
and `command_json` are the raw escape hatches for commands with other arities.
`command_pipeline_json` accepts a JSON array of command argument arrays and
returns a JSON array of normalized RESP responses. `argv_json*`,
`argv_json_list`, and `pipeline_json*` are typed builders for composing those
raw JSON command and pipeline payloads without hand-escaping nested JSON.
`response_type`, `response_value`, `response_int`, `response_bool`,
`response_double_text`, `response_bignumber_text`,
`response_blob_error_text`, `response_verbatim_text`, `response_nil`,
`response_ok`, `response_error`, `response_error_text`,
`response_array_value`, `response_array_length`, `response_set_value`,
`response_set_length`, `response_map_length`, `response_map_key_value`,
`response_map_value_value`, `response_map_value`, and `response_map_has_key` are
typed extractors for normalized RESP2/RESP3 JSON responses. Map/set extractors
cover scalar `value` fields; callers can keep raw JSON for nested structures.
`cluster_command_redirect_once_json` executes one raw JSON command and follows
one `MOVED` or `ASK` response once. `ASK` redirects send `ASKING` before the
retry. The helper is intentionally explicit and does not change ordinary
`command*` behavior. `cluster_command_redirect_retry_json` follows a bounded
chain of redirects for the same raw JSON command.
`cluster_command_slot_once_json` uses caller-provided normalized
`CLUSTER SLOTS` JSON plus `CLUSTER KEYSLOT` from the seed connection to route one
raw JSON command to the current slot owner.
`cluster_command_slot_retry_json` adds bounded redirect-chain retry after the
slot-owner command, which lets callers recover from a stale `CLUSTER SLOTS`
snapshot without keeping an internal cache.
`cluster_command_key_once_json` and `cluster_command_key_retry_json` refresh
`CLUSTER SLOTS` per call, compute the slot with `CLUSTER KEYSLOT`, and route one
raw JSON command by key.
The cluster password/auth raw helper variants authenticate temporary slot-owner
and redirect-target connections before executing the target command. They are
available for per-call slots refresh, caller-managed cached slots, and
cache-refresh-on-use routes.
`ClusterSlotsCache` lets callers hold a long-lived normalized `CLUSTER SLOTS`
snapshot and refresh it explicitly; cached key helpers route through that cache.
Cache-refresh key helpers refresh an empty cache before routing and can refresh
again after an exhausted redirect chain or retryable cluster errors such as
`READONLY`, `TRYAGAIN`, and `CLUSTERDOWN`. `cluster_retryable_error` exposes
that classification for callers that implement their own routing policy.
`cluster_get`, `cluster_get_or`, `cluster_set`, `cluster_set_nx`,
`cluster_set_xx`, `cluster_set_ex`, `cluster_mget_json`, `cluster_mset`,
`cluster_append`, `cluster_strlen`,
`cluster_del`, `cluster_exists`, `cluster_expire`, `cluster_persist`, `cluster_ttl`, `cluster_pttl`,
`cluster_incr`, `cluster_incr_by`, `cluster_decr`, and `cluster_decr_by` provide explicit cluster-aware string
wrappers over the cache-refresh route. `cluster_mget_json` and `cluster_mset`
route by the first key and are intended for caller-managed same-slot key sets.
`cluster_hget`, `cluster_hget_or`, `cluster_hset`, `cluster_hdel`,
`cluster_hexists`, `cluster_hlen`, `cluster_hstrlen`, and `cluster_hgetall_json` provide explicit cluster-aware
hash wrappers for single-key hash commands over the same route.
`cluster_lpush`, `cluster_rpush`, `cluster_lpop`, `cluster_lpop_or`,
`cluster_rpop`, `cluster_rpop_or`, `cluster_llen`, and `cluster_lrange_json`
provide explicit cluster-aware list wrappers for single-key list commands over
the same route.
`cluster_sadd`, `cluster_srem`, `cluster_sismember`, and
`cluster_smembers_json` provide explicit cluster-aware set wrappers for
single-key set commands over the same route.
`read_response_json` reads the next response from a connected socket without
sending a command and is used by the basic Pub/Sub helpers.

### 5.5 Common Command API

Connection and server:

```xl
func ping(Client client) as string
func ping(Client client, string message) as string
func auth(Client client, string password) as bool
func auth(Client client, string username, string password) as bool
func hello(Client client, int protocol) as string
func hello_auth(Client client, int protocol, string username, string password) as string
func select(Client client, int db) as bool
func dbsize(Client client) as int
func flushdb(Client client) as bool
```

Sentinel discovery:

```xl
func sentinel_masters_json(Client client) as string
func sentinel_master_json(Client client, string name) as string
func sentinel_get_master_addr_by_name_json(Client client, string name) as string
func sentinel_master_host(string addr_response_json) as string
func sentinel_master_port(string addr_response_json) as int
func sentinel_connect_master(Client sentinel, string name) as Client
func sentinel_connect_master(Client sentinel, string name, int timeout_ms) as Client
func sentinel_command_master_once_json(Client sentinel, string name, string argv_json) as string
func sentinel_command_master_once_json(Client sentinel, string name, string argv_json, int timeout_ms) as string
func sentinel_command_master_retry_json(Client sentinel, string name, string argv_json, int max_attempts) as string
func sentinel_command_master_retry_json(Client sentinel, string name, string argv_json, int max_attempts, int timeout_ms) as string
func sentinel_command_master_once_password_json(Client sentinel, string name, string password, string argv_json) as string
func sentinel_command_master_once_password_json(Client sentinel, string name, string password, string argv_json, int timeout_ms) as string
func sentinel_command_master_retry_password_json(Client sentinel, string name, string password, string argv_json, int max_attempts) as string
func sentinel_command_master_retry_password_json(Client sentinel, string name, string password, string argv_json, int max_attempts, int timeout_ms) as string
func sentinel_command_master_once_auth_json(Client sentinel, string name, string username, string password, string argv_json) as string
func sentinel_command_master_once_auth_json(Client sentinel, string name, string username, string password, string argv_json, int timeout_ms) as string
func sentinel_command_master_retry_auth_json(Client sentinel, string name, string username, string password, string argv_json, int max_attempts) as string
func sentinel_command_master_retry_auth_json(Client sentinel, string name, string username, string password, string argv_json, int max_attempts, int timeout_ms) as string
func sentinel_ping(Client sentinel, string name) as string
func sentinel_ping(Client sentinel, string name, string message) as string
func sentinel_select(Client sentinel, string name, int db) as bool
func sentinel_dbsize(Client sentinel, string name) as int
func sentinel_get(Client sentinel, string name, string key) as string
func sentinel_get_or(Client sentinel, string name, string key, string fallback) as string
func sentinel_set(Client sentinel, string name, string key, string value) as bool
func sentinel_set_nx(Client sentinel, string name, string key, string value) as bool
func sentinel_set_xx(Client sentinel, string name, string key, string value) as bool
func sentinel_set_ex(Client sentinel, string name, string key, string value, int seconds) as bool
func sentinel_mget_json(Client sentinel, string name, array<string> keys) as string
func sentinel_mset(Client sentinel, string name, array<string> key_values) as bool
func sentinel_append(Client sentinel, string name, string key, string value) as int
func sentinel_strlen(Client sentinel, string name, string key) as int
func sentinel_del(Client sentinel, string name, string key) as int
func sentinel_exists(Client sentinel, string name, string key) as bool
func sentinel_expire(Client sentinel, string name, string key, int seconds) as bool
func sentinel_persist(Client sentinel, string name, string key) as bool
func sentinel_ttl(Client sentinel, string name, string key) as int
func sentinel_pttl(Client sentinel, string name, string key) as int
func sentinel_incr(Client sentinel, string name, string key) as int
func sentinel_incr_by(Client sentinel, string name, string key, int amount) as int
func sentinel_decr(Client sentinel, string name, string key) as int
func sentinel_decr_by(Client sentinel, string name, string key, int amount) as int
func sentinel_hget(Client sentinel, string name, string key, string field) as string
func sentinel_hget_or(Client sentinel, string name, string key, string field, string fallback) as string
func sentinel_hset(Client sentinel, string name, string key, string field, string value) as int
func sentinel_hdel(Client sentinel, string name, string key, string field) as int
func sentinel_hexists(Client sentinel, string name, string key, string field) as bool
func sentinel_hlen(Client sentinel, string name, string key) as int
func sentinel_hstrlen(Client sentinel, string name, string key, string field) as int
func sentinel_hgetall_json(Client sentinel, string name, string key) as string
func sentinel_lpush(Client sentinel, string name, string key, string value) as int
func sentinel_rpush(Client sentinel, string name, string key, string value) as int
func sentinel_lpop(Client sentinel, string name, string key) as string
func sentinel_lpop_or(Client sentinel, string name, string key, string fallback) as string
func sentinel_rpop(Client sentinel, string name, string key) as string
func sentinel_rpop_or(Client sentinel, string name, string key, string fallback) as string
func sentinel_llen(Client sentinel, string name, string key) as int
func sentinel_lrange_json(Client sentinel, string name, string key, int start, int stop) as string
func sentinel_sadd(Client sentinel, string name, string key, string member) as int
func sentinel_srem(Client sentinel, string name, string key, string member) as int
func sentinel_sismember(Client sentinel, string name, string key, string member) as bool
func sentinel_smembers_json(Client sentinel, string name, string key) as string
func sentinel_replicas_json(Client client, string name) as string
func sentinel_sentinels_json(Client client, string name) as string
```

These helpers query an existing Sentinel connection and return normalized raw
JSON. `sentinel_connect_master` performs one explicit master address lookup and
connects to the returned Redis endpoint. `sentinel_command_master_once_json`
performs that lookup, executes one raw JSON command on the current master, and
closes the temporary connection. `sentinel_command_master_retry_json` repeats the
same lookup-and-command sequence for a bounded number of attempts when the
response is an error. The password/auth variants authenticate the temporary
current-master connection before executing the target raw command and then close
that connection. `sentinel_ping`, `sentinel_select`, and `sentinel_dbsize`
provide explicit current-master connection/server wrappers over that route.
`sentinel_get`, `sentinel_get_or`, `sentinel_set`,
`sentinel_set_nx`, `sentinel_set_xx`, `sentinel_set_ex`, `sentinel_mget_json`,
`sentinel_mset`, `sentinel_append`, `sentinel_strlen`, `sentinel_del`, `sentinel_exists`, `sentinel_expire`,
`sentinel_persist`, `sentinel_ttl`, `sentinel_pttl`, `sentinel_incr`,
`sentinel_incr_by`, `sentinel_decr`, and `sentinel_decr_by` are explicit
current-master string wrappers over the same bounded lookup route.
`sentinel_hget`, `sentinel_hget_or`, `sentinel_hset`, `sentinel_hdel`,
`sentinel_hexists`, `sentinel_hlen`, `sentinel_hstrlen`, and `sentinel_hgetall_json` provide explicit current-master
hash wrappers over the same route. `sentinel_lpush`, `sentinel_rpush`,
`sentinel_lpop`, `sentinel_lpop_or`, `sentinel_rpop`, `sentinel_rpop_or`,
`sentinel_llen`, and `sentinel_lrange_json` provide explicit current-master
list wrappers over the same route. `sentinel_sadd`, `sentinel_srem`, `sentinel_sismember`, and
`sentinel_smembers_json` provide explicit current-master set wrappers over the
same route. Background monitoring and failover-aware persistent connection
management remain future work.

Cluster discovery:

```xl
func cluster_info_json(Client client) as string
func cluster_nodes_json(Client client) as string
func cluster_slots_json(Client client) as string
func cluster_slots_node_host(string slots_response_json, int slot) as string
func cluster_slots_node_port(string slots_response_json, int slot) as int
func cluster_connect_slot(string slots_response_json, int slot) as Client
func cluster_connect_slot(string slots_response_json, int slot, int timeout_ms) as Client
func cluster_keyslot(Client client, string key) as int
func cluster_myid(Client client) as string
func cluster_redirect_type(string response_json) as string
func cluster_redirect_slot(string response_json) as int
func cluster_redirect_endpoint(string response_json) as string
func cluster_redirect_host(string response_json) as string
func cluster_redirect_port(string response_json) as int
func cluster_retryable_error(string response_json) as bool
func cluster_connect_redirect(string response_json) as Client
func cluster_connect_redirect(string response_json, int timeout_ms) as Client
func cluster_asking(Client client) as bool
```

These helpers query an existing cluster node. The slot helpers extract a primary
endpoint from normalized `CLUSTER SLOTS` JSON and can explicitly connect to that
endpoint for a known slot. They do not maintain a refreshed slot cache or route
ordinary commands across nodes. The redirect helpers parse normalized Redis error
JSON, open explicit redirect connections, and send `ASKING` so callers can
implement their own routing policy outside the package. `cluster_retryable_error`
recognizes transient Redis Cluster errors that are worth a bounded slots refresh
and retry.

String commands:

```xl
func get(Client client, string key) as string
func get_or(Client client, string key, string fallback) as string
func exists(Client client, string key) as bool
func set(Client client, string key, string value) as bool
func set_nx(Client client, string key, string value) as bool
func set_xx(Client client, string key, string value) as bool
func set_ex(Client client, string key, string value, int seconds) as bool
func mget_json(Client client, array<string> keys) as string
func mset(Client client, array<string> key_values) as bool
func append(Client client, string key, string value) as int
func strlen(Client client, string key) as int
func del(Client client, string key) as int
func expire(Client client, string key, int seconds) as bool
func persist(Client client, string key) as bool
func ttl(Client client, string key) as int
func pttl(Client client, string key) as int
func incr(Client client, string key) as int
func incr_by(Client client, string key, int amount) as int
func decr(Client client, string key) as int
func decr_by(Client client, string key, int amount) as int
```

Hash commands:

```xl
func hget(Client client, string key, string field) as string
func hget_or(Client client, string key, string field, string fallback) as string
func hset(Client client, string key, string field, string value) as int
func hdel(Client client, string key, string field) as int
func hexists(Client client, string key, string field) as bool
func hlen(Client client, string key) as int
func hstrlen(Client client, string key, string field) as int
func hgetall_json(Client client, string key) as string
```

List commands:

```xl
func lpush(Client client, string key, string value) as int
func rpush(Client client, string key, string value) as int
func lpop(Client client, string key) as string
func lpop_or(Client client, string key, string fallback) as string
func rpop(Client client, string key) as string
func rpop_or(Client client, string key, string fallback) as string
func llen(Client client, string key) as int
func lrange_json(Client client, string key, int start, int stop) as string
```

Set commands:

```xl
func sadd(Client client, string key, string member) as int
func srem(Client client, string key, string member) as int
func sismember(Client client, string key, string member) as bool
func smembers_json(Client client, string key) as string
```

Pub/Sub:

```xl
func subscribe(Client client, string channel) as string
func unsubscribe(Client client, string channel) as string
func publish(Client client, string channel, string message) as int
func read_response_json(Client client) as string
```

## 6. RESP2 Protocol

### 6.1 Encoding

All commands are encoded as RESP arrays of bulk strings:

```text
*<argc>\r\n
$<len>\r\n
<arg>\r\n
...
```

Example:

```text
*3\r\n$3\r\nSET\r\n$4\r\nname\r\n$3\r\nAda\r\n
```

Implementation helpers:

```xl
func resp_encode1(string a0) as string
func resp_encode2(string a0, string a1) as string
func resp_encode3(string a0, string a1, string a2) as string
func resp_encode4(string a0, string a1, string a2, string a3) as string
func resp_encode_json(string argv_json) as string
func resp_encode_pipeline_json(string commands_json) as string
```

### 6.2 Decoding

V1 parser must support:

- simple string: `+OK\r\n`
- error: `-ERR ...\r\n`
- integer: `:1\r\n`
- bulk string: `$5\r\nhello\r\n`
- nil bulk: `$-1\r\n`
- array: `*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n`
- nil array: `*-1\r\n`
- RESP3 nil map: `%-1\r\n`
- RESP3 nil set: `~-1\r\n`

The implementation uses module-level parser state because the current C backend
rejected class object parser parameters during implementation. The intended
state shape remains:

```xl
class Reader extends object
  var buffer as string
  var pos as int

  func append(string chunk) as void
  func available() as int
  func parse_one_json() as string
end
```

`parse_one_json()` returns empty string when the buffered data does not contain a
full response yet. Protocol errors should return an error JSON and set the
client error.

### 6.3 RESP3 Response Parsing

V1 still encodes commands as RESP arrays of bulk strings and does not issue
`HELLO 3` automatically. The parser accepts common RESP3 response frames so raw
helpers can consume Redis servers or proxies that return RESP3-shaped values:

- null: `_\r\n`
- boolean: `#t\r\n` and `#f\r\n`
- double: `,3.14\r\n`
- big number: `(<digits>\r\n`
- blob error: `!<len>\r\n<body>\r\n`
- verbatim string: `=<len>\r\n<body>\r\n`
- map: `%<count>\r\n<key><value>...`
- set: `~<count>\r\n<value>...`

RESP3 values are normalized into JSON objects with `type` values `null`,
`bool`, `double`, `bignumber`, `blob_error`, `verbatim`, `map`, and `set`.
Callers that want RESP3 can explicitly call `hello(client, 3)` or
`hello_auth(client, 3, username, password)`.

### 6.4 Receive Loop

Command execution:

1. Encode command.
2. Send encoded bytes with `tcp.send`.
3. Repeatedly call `tcp.recv(socket, recv_size, timeout_ms)`.
4. Append chunks to `Reader`.
5. Stop after one full response is parsed.
6. On timeout or empty receive before response, set `last_error`.

Recommended values:

- `recv_size`: `4096`
- maximum response buffer: `16 MB` by default
- maximum nested array depth: `16`

These limits avoid unbounded memory growth in a pure source V1.

## 7. Error Model

Every API that maps to Redis command should follow these rules:

- Redis `-ERR` response is not thrown; it records `last_error` and returns the
  command-specific failure value.
- Transport failure sets `last_error` and marks the client invalid if the socket
  is unusable.
- Protocol parse failure sets `last_error` and closes the connection.
- Boolean wrappers return `false` on Redis error, transport error, and parse
  error.
- Integer wrappers return `-1` on failure unless Redis itself can validly return
  `-1`, such as `ttl`. For those commands, document the ambiguity and provide
  `*_json` raw access.

## 8. Offline Tests

Default regression should cover pure functions without Redis:

- RESP encode helpers.
- Simple string parser.
- Error parser.
- Integer parser.
- Bulk string parser.
- Nil bulk parser.
- Array parser.
- Split response across multiple chunks.
- Malformed response detection.
- JSON normalization shape.

Example:

```text
demo5/release/examples/redis/redis_offline_codec.xl
```

This example should return `0` on success and non-zero stable codes on failure.

## 9. Real Smoke Tests

Real Redis tests must be opt-in. Required environment variables:

```text
XL_REDIS_HOST      default 127.0.0.1
XL_REDIS_PORT      default 6379
XL_REDIS_PASSWORD  optional
XL_REDIS_DB        default 0
XL_REDIS_REAL      must be 1 to run real smoke
```

Real smoke should:

1. skip clearly when `XL_REDIS_REAL` is not `1`;
2. connect to Redis;
3. authenticate if password is set;
4. select configured DB;
5. use a unique key prefix such as `xlang:smoke:<timestamp>`;
6. test `PING`, `SET`, `GET`, `EXPIRE`, `DEL`, `HSET`, `HGET`, `LPUSH`, `RPOP`;
7. clean up keys before exiting.

No default regression should mutate a developer's Redis instance.

## 10. Implementation Order

1. Create package skeleton and metadata.
2. Implement RESP encoder.
3. Implement offline parser and JSON normalizer.
4. Implement `Client`, `connect`, `close`, and command send/receive.
5. Implement string/server wrappers.
6. Add hash/list/set wrappers.
7. Add offline examples and optional real smoke scripts.
8. Update release docs or package index if the project adds one later.

## 11. Compatibility and Future Backends

The high-level API should not expose the transport implementation. Future
versions can add:

- `x.redis.native` based on a C RESP client;
- TLS support;
- connection pooling is available as a fixed-size idle socket pool;
- Sentinel discovery helpers are available for querying an existing Sentinel
  connection, and `sentinel_connect_master` can explicitly connect to the
  current master; one-shot current-master command execution is available through
  `sentinel_command_master_once_json`, with bounded error retry available
  through `sentinel_command_master_retry_json`; explicit current-master string
  wrappers are available as `sentinel_get`, `sentinel_get_or`, `sentinel_set`,
  `sentinel_set_nx`, `sentinel_set_xx`, `sentinel_set_ex`, `sentinel_mget_json`,
  `sentinel_mset`, `sentinel_append`, `sentinel_strlen`, `sentinel_del`, `sentinel_exists`, `sentinel_expire`, `sentinel_persist`, `sentinel_ttl`, `sentinel_pttl`,
  `sentinel_incr`, `sentinel_incr_by`, `sentinel_decr`, and `sentinel_decr_by`; explicit current-master hash wrappers are available as
  `sentinel_hget`, `sentinel_hget_or`, `sentinel_hset`, `sentinel_hdel`, `sentinel_hexists`, `sentinel_hlen`, `sentinel_hstrlen`, and
  `sentinel_hgetall_json`; explicit current-master list wrappers are available
  as `sentinel_lpush`, `sentinel_rpush`, `sentinel_lpop`, `sentinel_lpop_or`,
  `sentinel_rpop`, `sentinel_rpop_or`, `sentinel_llen`, and `sentinel_lrange_json`; explicit current-master set
  wrappers are available as `sentinel_sadd`, `sentinel_srem`,
  `sentinel_sismember`, and `sentinel_smembers_json`; automatic failover-aware
  persistent connection management remains future work;
- Cluster discovery helpers are available for querying an existing cluster node;
  `MOVED`/`ASK` error parsing, explicit redirect connect, and `ASKING` are
  available; one-shot raw command redirect retry is available through
  `cluster_command_redirect_once_json`, and bounded redirect-chain retry is
  available through `cluster_command_redirect_retry_json`; explicit
  `CLUSTER SLOTS` endpoint lookup and one-shot slot-owner command routing are
  available, with bounded stale-slot redirect recovery through
  `cluster_command_slot_retry_json`; per-call slots refresh and key routing are
  available through `cluster_command_key_once_json` and
  `cluster_command_key_retry_json`; explicit long-lived slot cache and cached key
  routing are available through `ClusterSlotsCache`, including explicit
  cache-refresh-on-use helpers, same-slot `MGET`/`MSET`, and cluster-aware
  string/hash/list/set wrappers;
  transparent cross-node routing for ordinary commands remains future work;
- basic pub/sub helpers are available through `subscribe`, `unsubscribe`,
  `publish`, and `read_response_json`; event-loop integrations remain future
  work;
- pipeline API is available through `command_pipeline_json` and typed JSON
  builders `argv_json*`, `argv_json_list`, and `pipeline_json*`;
- RESP3 parsing is available for common response types, explicit `HELLO`
  helpers, and typed response extractors; automatic RESP3 negotiation remains
  future work;
- cluster-aware routing.

The same `Client` class can carry a native handle later if needed. Public
wrappers should remain source-compatible.

`transport_name`, `transport_binary_safe`, `transport_tls`, and
`transport_limits_json` expose the current transport boundary in code. The V1
values are `std.net.tcp`, `false`, `false`, and a JSON object describing the
string-oriented TCP API. A separate Redis transport library can use these values
to decide whether to replace `x.redis` for workloads that require binary-safe
bulk strings, TLS sockets, or connect timeout control.
`sentinel_capabilities_json` and `cluster_capabilities_json` expose the explicit
Sentinel/Cluster support level in code, including the remaining transparent
automation limits.
The concrete handoff contract for the remaining blocked work is tracked in
`dev/extlibs/redis/x.redis-blocked-handoff-spec.md`.

## 12. Risks and Mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| `std.net.tcp.recv` is text-oriented | Binary values and huge responses are unsafe | Document V1 as text-safe Redis client; defer binary-safe mode to native backend |
| No connect timeout API | Bad host may block longer than desired | Document limitation; add native backend later if needed |
| RESP parser complexity in xlang | Bugs in nested arrays or chunking | Keep parser small, add offline chunk tests first |
| JSON construction by string concatenation | Escaping bugs | Implement one local JSON string escape helper and test quotes/backslashes/control chars |
| Real smoke mutates Redis | User data risk | Require `XL_REDIS_REAL=1`, unique key prefix, cleanup, no default regression dependency |
| Integer failure sentinel conflicts | `ttl` can return `-1` and `-2` | Provide raw JSON APIs and document wrapper behavior |

## 13. V1 Done Definition

V1 source-package scope is complete when:

- [X] `demo5/release/libs/x.redis/package.json` exists and resolves via `import x.redis as redis`;
- [X] `redis_offline_codec.xl` passes without external services;
- [X] `redis_basic.xl` can pass against fake Redis and opt-in real Redis instances;
- [X] common wrappers listed in section 5.5 are implemented or explicitly marked
  deferred in the tracker;
- [X] README documents V1 transport limits and real smoke environment variables;
- [X] default regression remains independent of Redis availability;
- [X] remaining blocked work has concrete handoff contracts in
  `dev/extlibs/redis/x.redis-blocked-handoff-spec.md`.
