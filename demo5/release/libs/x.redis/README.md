# x.redis

Pure xlang Redis client package.

V1 uses `std.net.tcp` directly and is intended for text-safe Redis command
usage. It sends RESP2 commands and can parse RESP2 plus common RESP3 response
types. Basic explicit Cluster and Sentinel helpers are included. Binary-safe
bulk strings, TLS sockets, native backends, transparent Cluster/Sentinel
connection management, and pub/sub event-loop integration are deferred.

## Usage

```xl
import x.redis as redis

func main() as int
	auto c = redis.connect("127.0.0.1", 6379)
	if c.valid() == false
		return 1
	end

	if redis.ping(c) != "PONG"
		return 2
	end

	if redis.set(c, "xlang:hello", "world") == false
		return 3
	end
	if redis.get(c, "xlang:hello") != "world"
		return 4
	end

	redis.del(c, "xlang:hello")
	redis.close(c)
	return 0
end
```

## Commands

V1 wraps these commands:

- Connection/server: `ping`, `auth`, `hello`, `hello_auth`, `select`, `dbsize`,
  `flushdb`.
- Sentinel discovery: `sentinel_masters_json`, `sentinel_master_json`,
  `sentinel_get_master_addr_by_name_json`, `sentinel_master_host`,
  `sentinel_master_port`, `sentinel_connect_master`,
  `sentinel_command_master_once_json`, `sentinel_command_master_retry_json`,
  `sentinel_command_master_once_password_json`,
  `sentinel_command_master_retry_password_json`,
  `sentinel_command_master_once_auth_json`,
  `sentinel_command_master_retry_auth_json`,
  `sentinel_ping`, `sentinel_select`, `sentinel_dbsize`, `sentinel_get`,
  `sentinel_get_or`, `sentinel_set`, `sentinel_set_nx`,
  `sentinel_set_xx`, `sentinel_set_ex`, `sentinel_mget_json`,
  `sentinel_mset`, `sentinel_append`, `sentinel_strlen`, `sentinel_del`,
  `sentinel_exists`, `sentinel_expire`, `sentinel_persist`,
  `sentinel_ttl`, `sentinel_pttl`, `sentinel_incr`, `sentinel_incr_by`,
  `sentinel_decr`, `sentinel_decr_by`,
  `sentinel_hget`, `sentinel_hget_or`, `sentinel_hset`, `sentinel_hdel`,
  `sentinel_hexists`, `sentinel_hlen`, `sentinel_hstrlen`, `sentinel_hgetall_json`,
  `sentinel_lpush`, `sentinel_rpush`, `sentinel_lpop`, `sentinel_lpop_or`,
  `sentinel_rpop`, `sentinel_rpop_or`, `sentinel_llen`, `sentinel_lrange_json`,
  `sentinel_sadd`, `sentinel_srem`, `sentinel_sismember`,
  `sentinel_smembers_json`,
  `sentinel_replicas_json`, `sentinel_sentinels_json`.
- Cluster discovery: `cluster_info_json`, `cluster_nodes_json`,
  `cluster_slots_json`, `cluster_slots_node_host`, `cluster_slots_node_port`,
  `cluster_connect_slot`, `cluster_slots_cache`, `cluster_slots_cache_json`,
  `cluster_slots_cache_valid`, `cluster_slots_cache_refresh`,
  `cluster_keyslot`, `cluster_myid`,
  `cluster_redirect_type`, `cluster_redirect_slot`,
  `cluster_redirect_endpoint`, `cluster_redirect_host`,
  `cluster_redirect_port`, `cluster_retryable_error`,
  `cluster_connect_redirect`, `cluster_asking`.
- Pool: `pool`, `pool_acquire`, `pool_release`, `pool_close`, `pool_idle_count`.
- Strings: `get`, `get_or`, `exists`, `set`, `set_nx`, `set_xx`, `set_ex`,
  `mget_json`, `mset`, `del`, `expire`, `persist`, `ttl`, `pttl`, `incr`,
  `incr_by`, `decr`, `decr_by`, `append`, `strlen`.
- Hashes: `hget`, `hget_or`, `hset`, `hdel`, `hexists`, `hlen`, `hstrlen`, `hgetall_json`.
- Lists: `lpush`, `rpush`, `lpop`, `lpop_or`, `rpop`, `rpop_or`, `llen`, `lrange_json`.
- Sets: `sadd`, `srem`, `sismember`, `smembers_json`.
- Pub/Sub: `subscribe`, `unsubscribe`, `publish`, `read_response_json`.
- Raw commands: `command1` through `command5`, `command_array`, and `command_json`.
- Cluster raw command retry: `cluster_command_redirect_once_json`,
  `cluster_command_redirect_retry_json`, `cluster_command_slot_once_json`,
  `cluster_command_slot_retry_json`, `cluster_command_key_once_json`,
  `cluster_command_key_retry_json`,
  `cluster_command_slot_once_password_json`,
  `cluster_command_slot_retry_password_json`,
  `cluster_command_slot_once_auth_json`,
  `cluster_command_slot_retry_auth_json`,
  `cluster_command_key_once_password_json`,
  `cluster_command_key_retry_password_json`,
  `cluster_command_key_once_auth_json`,
  `cluster_command_key_retry_auth_json`,
  `cluster_command_cached_key_once_json`,
  `cluster_command_cached_key_retry_json`,
  `cluster_command_cache_refresh_key_once_json`,
  `cluster_command_cache_refresh_key_retry_json`,
  `cluster_command_cached_key_once_password_json`,
  `cluster_command_cached_key_retry_password_json`,
  `cluster_command_cached_key_once_auth_json`,
  `cluster_command_cached_key_retry_auth_json`,
  `cluster_command_cache_refresh_key_once_password_json`,
  `cluster_command_cache_refresh_key_retry_password_json`,
  `cluster_command_cache_refresh_key_once_auth_json`,
  `cluster_command_cache_refresh_key_retry_auth_json`.
- Cluster strings: `cluster_get`, `cluster_get_or`, `cluster_set`,
  `cluster_set_nx`, `cluster_set_xx`, `cluster_set_ex`, `cluster_mget_json`,
  `cluster_mset`, `cluster_del`, `cluster_append`, `cluster_strlen`,
  `cluster_exists`, `cluster_expire`, `cluster_persist`, `cluster_ttl`,
  `cluster_pttl`, `cluster_incr`, `cluster_incr_by`,
  `cluster_decr`, `cluster_decr_by`.
- Cluster hashes: `cluster_hget`, `cluster_hget_or`, `cluster_hset`,
  `cluster_hdel`, `cluster_hexists`, `cluster_hlen`, `cluster_hstrlen`, `cluster_hgetall_json`.
- Cluster lists: `cluster_lpush`, `cluster_rpush`, `cluster_lpop`,
  `cluster_lpop_or`, `cluster_rpop`, `cluster_rpop_or`, `cluster_llen`,
  `cluster_lrange_json`.
- Cluster sets: `cluster_sadd`, `cluster_srem`, `cluster_sismember`,
  `cluster_smembers_json`.
- RESP helpers: `resp_encode1` through `resp_encode5`, `resp_encode_array`,
  `resp_encode_json`, `resp_encode_pipeline_json`, `parse_response_json`,
  `parse_response_chunks_json`, `transport_name`, `transport_binary_safe`,
  `transport_tls`, `transport_limits_json`, `sentinel_capabilities_json`,
  `cluster_capabilities_json`, `argv_json1` through `argv_json4`,
  `argv_json_list`, `pipeline_json_array`, and `pipeline_json1` through
  `pipeline_json5`.
- Response helpers: `response_type`, `response_value`, `response_int`,
  `response_bool`, `response_double_text`, `response_bignumber_text`,
  `response_blob_error_text`, `response_verbatim_text`, `response_nil`,
  `response_ok`, `response_error`, `response_error_text`,
  `response_array_value`, `response_array_length`, `response_set_value`,
  `response_set_length`, `response_map_length`, `response_map_key_value`,
  `response_map_value_value`, `response_map_value`, `response_map_has_key`.
- Pipeline: `command_pipeline_json`.

Raw command helpers return normalized RESP JSON:

```json
{"type":"simple","value":"OK"}
{"type":"error","value":"ERR unknown command"}
{"type":"integer","value":1}
{"type":"bulk","value":"hello","nil":false}
{"type":"bulk","value":"","nil":true}
{"type":"array","items":[{"type":"bulk","value":"a","nil":false}],"nil":false}
{"type":"null","nil":true}
{"type":"bool","value":true}
{"type":"double","value":"3.14"}
{"type":"bignumber","value":"3492890328409238509324850943850943825024385"}
{"type":"blob_error","value":"SYNTAX invalid syntax"}
{"type":"verbatim","value":"txt:Some string"}
{"type":"map","pairs":[{"key":{"type":"simple","value":"name"},"value":{"type":"bulk","value":"Ada","nil":false}}],"nil":false}
{"type":"set","items":[{"type":"simple","value":"red"}],"nil":false}
```

Use `response_type`, `response_value`, `response_int`, `response_bool`,
`response_double_text`, `response_bignumber_text`,
`response_blob_error_text`, `response_verbatim_text`, `response_nil`,
`response_ok`, `response_error`, `response_error_text`,
`response_array_value`, `response_array_length`, `response_set_value`,
`response_set_length`, `response_map_length`, `response_map_key_value`,
`response_map_value_value`, `response_map_value`, and `response_map_has_key` to
read normalized responses without repeating JSON field access in callers.
Map/set helpers target the common case where items have a scalar `value`; keep
raw JSON for nested structures.

`command_pipeline_json(client, "[[\"SET\",\"k\",\"v\"],[\"GET\",\"k\"]]")`
sends all commands in one TCP write and returns a JSON array of normalized RESP
responses. Use the JSON builders to avoid hand-escaping nested pipeline JSON:

```xl
auto commands = redis.pipeline_json2(redis.argv_json3("SET", "k", "v"), redis.argv_json2("GET", "k"))
auto responses = redis.command_pipeline_json(client, commands)
```

`hello(client, 3)` and `hello_auth(client, 3, "default", "password")` expose
Redis `HELLO` as raw JSON helpers. V1 still defaults to RESP2 until callers
explicitly negotiate another protocol.

Sentinel helpers query an existing Sentinel connection and return normalized raw
JSON. `sentinel_connect_master` queries `GET-MASTER-ADDR-BY-NAME` and opens a
Redis connection to that address. `sentinel_command_master_once_json` performs
that lookup, executes one raw JSON command on the current master, and closes the
temporary master connection. `sentinel_command_master_retry_json` repeats that
lookup-and-command sequence for a bounded number of attempts when the response is
an error. The `*_password_json` variants authenticate with `AUTH password`
after opening the temporary master connection, and the `*_auth_json` variants
authenticate with `AUTH username password`; both close the temporary connection
after the target command. `sentinel_get`, `sentinel_get_or`, `sentinel_set`,
`sentinel_set_nx`, `sentinel_set_xx`, `sentinel_set_ex`, `sentinel_mget_json`,
`sentinel_mset`, `sentinel_append`, `sentinel_strlen`, `sentinel_del`, `sentinel_exists`, `sentinel_expire`, `sentinel_persist`,
`sentinel_ttl`, `sentinel_pttl`, `sentinel_incr`, `sentinel_incr_by`,
`sentinel_decr`, and `sentinel_decr_by` are explicit current-master string
wrappers over the same bounded lookup route.
`sentinel_ping`, `sentinel_select`, and `sentinel_dbsize` provide explicit
current-master connection/server wrappers over that route.
`sentinel_hget`, `sentinel_hget_or`, `sentinel_hset`, `sentinel_hdel`,
`sentinel_hexists`, `sentinel_hlen`, `sentinel_hstrlen`, and `sentinel_hgetall_json` provide the same explicit
current-master route for single-key hash operations.
`sentinel_lpush`, `sentinel_rpush`, `sentinel_lpop`, `sentinel_lpop_or`,
`sentinel_rpop`, `sentinel_rpop_or`, `sentinel_llen`, and
`sentinel_lrange_json` do the same for single-key list operations.
`sentinel_sadd`, `sentinel_srem`, `sentinel_sismember`, and
`sentinel_smembers_json` do the same for single-key set operations.
V1 does not yet perform background monitoring or failover inside ordinary
`connect`.

Cluster helpers query an existing cluster node and return normalized raw JSON or
simple mapped values. `cluster_slots_node_host` and `cluster_slots_node_port`
extract a primary endpoint from normalized `CLUSTER SLOTS` JSON for a known slot,
and `cluster_connect_slot` explicitly connects to that endpoint.
`cluster_redirect_*` parses `MOVED` and `ASK` error JSON into type, slot,
endpoint, host, and port. `cluster_connect_redirect` explicitly opens a
connection to the redirect target, and `cluster_asking` sends `ASKING` for
caller-managed ASK retries. `cluster_command_redirect_once_json` executes a raw
JSON command and follows one `MOVED` or `ASK` response once.
`cluster_command_redirect_retry_json` follows a bounded chain of redirects. V1
also exposes `cluster_command_slot_once_json`, which uses a provided
`CLUSTER SLOTS` response plus `CLUSTER KEYSLOT` to route one raw JSON command to
the current slot owner. `cluster_command_slot_retry_json` additionally follows a
bounded redirect chain after the slot-owner command, which is useful when the
provided slots response is stale. `cluster_command_key_once_json` and
`cluster_command_key_retry_json` refresh `CLUSTER SLOTS` per call before routing
one raw JSON command by key. The `*_password_json` and `*_auth_json` cluster raw
helpers authenticate temporary slot-owner and redirect-target connections before
executing the target command, including per-call slots refresh, cached slots, and
cache-refresh-on-use routes. `ClusterSlotsCache` plus `cluster_slots_cache_*`
lets callers hold and refresh a long-lived slots snapshot explicitly, and the
cached key helpers route through it. The cache-refresh key helpers refresh an
empty cache before routing and can refresh again after an exhausted redirect
chain or after a retryable cluster error such as `READONLY`, `TRYAGAIN`, or
`CLUSTERDOWN`. `cluster_retryable_error` exposes that error classification for
callers that build their own routing policy. `cluster_get`, `cluster_get_or`,
`cluster_set`, `cluster_set_nx`, `cluster_set_xx`, `cluster_set_ex`,
`cluster_mget_json`, `cluster_mset`, `cluster_append`, `cluster_strlen`,
`cluster_del`, `cluster_exists`, `cluster_expire`, `cluster_persist`,
`cluster_ttl`, `cluster_pttl`, `cluster_incr`, `cluster_incr_by`,
`cluster_decr`, and `cluster_decr_by` are explicit cluster-aware
string wrappers over this cache-refresh route. `cluster_hget`,
`cluster_hget_or`, `cluster_hset`, `cluster_hdel`, `cluster_hexists`, `cluster_hlen`, `cluster_hstrlen`, and
`cluster_hgetall_json` provide the same explicit route for single-key hash
operations. `cluster_lpush`, `cluster_rpush`, `cluster_lpop`,
`cluster_lpop_or`, `cluster_rpop`, `cluster_rpop_or`, `cluster_llen`, and
`cluster_lrange_json` do the same for single-key list operations.
`cluster_sadd`, `cluster_srem`, `cluster_sismember`, and
`cluster_smembers_json` do the same for single-key set operations.
`cluster_mget_json` and `cluster_mset` route by the first key and are intended
for caller-managed same-slot key sets, usually with Redis hash tags such as
`{tenant42}`. Cross-slot multi-key commands remain the caller's responsibility.
V1 does not yet provide transparent cross-node routing for ordinary commands.

`subscribe` returns the Redis subscription acknowledgement. After that, call
`read_response_json` on the subscribed client to receive message arrays. V1 does
not include an event loop; callers own the blocking read cadence and timeout.

The connection pool is a small fixed-slot idle pool. Release clients with
`pool_release(pool, move(client))`; after release, the moved client must not be
used by the caller. V1 keeps at most four idle sockets per pool.

`flushdb` is part of public V1 for parity with Redis, but it is destructive.
Do not call it from default regression or shared Redis instances.

## Offline Test

```bat
release\xl.exe run release\examples\redis\redis_offline_codec.xl
```

An optional fake Redis smoke can exercise `redis_basic.xl` end to end without a
Redis installation. It requires Node.js and starts an in-process RESP2 test
server:

```bat
dev\extlibs\redis\run_x_redis_fake_smoke.bat
```

Run all default x.redis checks without a real Redis instance:

```bat
dev\extlibs\redis\run_x_redis_all_checks.bat
```

```sh
dev/extlibs/redis/run_x_redis_all_checks.sh
```

These checks also validate that package metadata exports match the source
module exports.

## Limits

- `std.net.tcp.connect` does not expose a connect timeout.
- `std.net.tcp.recv` returns `string`, so V1 is not binary-safe.
- `std.net.tls` currently exposes TLS metadata/config helpers only; it does not
  expose TLS socket connect/send/recv/close APIs for Redis transport.
- `transport_name`, `transport_binary_safe`, `transport_tls`, and
  `transport_limits_json` expose these transport capabilities for callers that
  need to choose between `x.redis` and a separate native Redis transport.
- `sentinel_capabilities_json` and `cluster_capabilities_json` expose the
  explicit Sentinel/Cluster support level and transparent automation limits.
- The implementation handoff contract for the remaining blocked work is tracked
  in `dev/extlibs/redis/x.redis-blocked-handoff-spec.md`.
- Default command receive timeout is 5000 ms.
- Real Redis smoke tests are opt-in and should use a disposable key prefix.

## Real Smoke Environment

```text
XL_REDIS_REAL=1
XL_REDIS_HOST=127.0.0.1
XL_REDIS_PORT=6379
XL_REDIS_PASSWORD=
XL_REDIS_DB=0
```

Run the opt-in smoke from the repository root:

```bat
dev\extlibs\redis\run_x_redis_smoke.bat
```
