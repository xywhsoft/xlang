# x.redis Implementation Tracker

Status legend:

- [ ] not started
- [X] complete
- [~] in progress
- [!] blocked or needs decision

Implementation target: `demo5/release/libs/x.redis`
Strategy: pure xlang source package over `std.net.tcp`
Current status: [X] V1 source package implemented, default all-checks verified, and real Redis smoke available as opt-in validation.

## Scope Decisions

- [X] V1 uses pure xlang source implementation.
- [X] V1 uses `std.net.tcp` for transport.
- [X] V1 sends RESP2 commands and parses RESP2 plus common RESP3 response types.
- [X] V1 package name is `x.redis`.
- [X] Native C transport work is deferred.
- [X] Third-party C client integrations are out of scope for `x.redis`; they will be maintained as separate libraries.
- [X] Default regression must not require a running Redis server.
- [X] Confirm whether `x.redis` should later be promoted into a release bundle list if the project adds package indexing. Current release layout discovers packages from `demo5/release/libs/<name>/package.json`; no separate index is present.

## Package Skeleton

- [X] Create `demo5/release/libs/x.redis/package.json`.
- [X] Create `demo5/release/libs/x.redis/README.md`.
- [X] Create `demo5/release/libs/x.redis/src/redis.xl`.
- [X] Set package metadata to `x.redis@0.1.0#abi1`.
- [X] Export module key `x.redis`.
- [X] Import `std.net.tcp as tcp` inside the package source.
- [X] Add public export list matching implemented functions.
- [X] Add public constants to package metadata exports.

Acceptance:

- [X] `import x.redis as redis` resolves from `release/libs`.
- [X] Package has no native `cSources`.

## RESP Encoder

- [X] Implement integer-to-string length formatting for bulk strings.
- [X] Implement `resp_encode1`.
- [X] Implement `resp_encode2`.
- [X] Implement `resp_encode3`.
- [X] Implement `resp_encode4`.
- [X] Implement `resp_encode5` if needed by common wrappers.
- [X] Implement `resp_encode_json` if JSON argument parsing is practical with current stdlib.
- [X] Confirm current xlang JSON parse ergonomics before committing `command_json` as mandatory V1 API.

Acceptance:

- [X] `PING` encodes as `*1\r\n$4\r\nPING\r\n`.
- [X] `SET name Ada` encodes as `*3\r\n$3\r\nSET\r\n$4\r\nname\r\n$3\r\nAda\r\n`.
- [X] Empty string encodes as `$0\r\n\r\n`.

## JSON Normalization Helpers

- [X] Implement JSON string escape helper. Implemented through `std.json.encode`.
- [X] Escape quotes.
- [X] Escape backslashes.
- [X] Escape CR/LF/TAB.
- [X] Decide how to handle other control characters. Delegated to `std.json.encode`.
- [X] Implement response JSON builders for `simple`, `error`, `integer`, `bulk`, and `array`.
- [X] Implement RESP3 response JSON builders for `null`, `bool`, `double`, `bignumber`, `blob_error`, `verbatim`, `map`, and `set`.
- [X] Export normalized response extractors `response_type`, `response_value`, `response_int`, `response_bool`, `response_double_text`, `response_bignumber_text`, `response_blob_error_text`, `response_verbatim_text`, `response_nil`, `response_ok`, `response_error`, `response_error_text`, `response_array_value`, `response_array_length`, `response_set_value`, `response_set_length`, `response_map_length`, `response_map_key_value`, `response_map_value_value`, `response_map_value`, and `response_map_has_key`.
- [X] Export transport capability helpers `transport_name`, `transport_binary_safe`, `transport_tls`, and `transport_limits_json`.
- [X] Export Sentinel/Cluster capability helpers `sentinel_capabilities_json` and `cluster_capabilities_json`.
- [X] Confirm whether current xlang supports enough string character inspection for full JSON escaping. Not needed because `std.json.encode` handles string escaping.

Acceptance:

- [X] Values containing `"` and `\` produce valid JSON.
- [X] Bulk nil is represented as `{"type":"bulk","value":"","nil":true}`.
- [X] Redis error is represented as `{"type":"error","value":"..."}`.

## RESP Parser

- [X] Create `Reader` class with `buffer` and `pos`. Implemented as module-level parser state because current C backend rejected class object parser parameters.
- [X] Implement `append(chunk)`.
- [X] Implement line reading through CRLF.
- [X] Implement simple string parser.
- [X] Implement error parser.
- [X] Implement integer parser.
- [X] Implement bulk string parser.
- [X] Implement nil bulk parser.
- [X] Implement array parser.
- [X] Implement nil array parser.
- [X] Implement RESP3 null parser.
- [X] Implement RESP3 bool parser.
- [X] Implement RESP3 double parser.
- [X] Implement RESP3 big number parser.
- [X] Implement RESP3 blob error parser.
- [X] Implement RESP3 verbatim string parser.
- [X] Implement RESP3 map parser.
- [X] Implement RESP3 nil map parser.
- [X] Implement RESP3 set parser.
- [X] Implement RESP3 nil set parser.
- [X] Support split responses across multiple `append` calls. Command receive loop appends chunks; `parse_response_chunks_json` covers the offline parser path.
- [X] Enforce maximum nesting depth.
- [X] Enforce maximum buffered response size.
- [X] Return incomplete state without losing buffered data.
- [X] Detect malformed prefix.
- [X] Detect missing CRLF after bulk body.

Acceptance:

- [X] `+OK\r\n` parses to simple JSON.
- [X] `-ERR bad\r\n` parses to error JSON.
- [X] `:12\r\n` parses to integer JSON.
- [X] `$5\r\nhello\r\n` parses to bulk JSON.
- [X] `$-1\r\n` parses to nil bulk JSON.
- [X] `*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n` parses to array JSON.
- [X] `*-1\r\n` parses to nil array JSON.
- [X] `%-1\r\n` parses to RESP3 nil map JSON.
- [X] `~-1\r\n` parses to RESP3 nil set JSON.
- [X] Invalid RESP prefixes return normalized error JSON.
- [X] Invalid RESP3 null frames return normalized error JSON.
- [X] Invalid array, map, and set lengths return normalized error JSON.
- [X] Split chunks `"$5\r\nhe"` and `"llo\r\n"` produce one complete response.
- [X] Response extractors read simple, error, integer, nil, array item/length, RESP3 bool/double/bignumber/blob_error/verbatim, RESP3 set item/length, and RESP3 map key/value/length normalized JSON by index and by scalar key.
- [X] Transport capability helpers report current `std.net.tcp` limits programmatically.
- [X] Sentinel/Cluster capability helpers report explicit support level and remaining transparent automation limits programmatically.

## Client Lifecycle

- [X] Implement `Client` class.
- [X] Store socket handle.
- [X] Store host, port, timeout, and last error text.
- [X] Implement destructor close fallback.
- [X] Implement `valid()`.
- [X] Implement `close()`.
- [X] Implement `last_error()`.
- [X] Implement `set_timeout(ms)`.
- [X] Implement `connect(host, port)`.
- [X] Implement `connect(host, port, timeout_ms)`.
- [X] Handle failed `tcp.connect` with readable error.
- [X] Implement fixed-slot `Pool`.

Acceptance:

- [X] Invalid connection returns a client whose `valid()` is `false`, or documented null-equivalent behavior.
- [X] Calling `close()` twice does not crash.
- [X] Destroying a live client closes the socket.
- [X] Failed `tcp.connect` exception is caught and converted into invalid client state.
- [X] Command on a closed client returns error JSON and records `last_error`.
- [X] Pool acquire/release reuses an idle socket and closes idle sockets.

## Command Execution

- [X] Implement low-level send helper.
- [X] Verify `tcp.send` wrote the full encoded command length.
- [X] Implement receive loop using `tcp.recv(socket, 4096, timeout_ms)`.
- [X] Feed chunks into parser state.
- [X] Return after one full response.
- [X] Set last error on timeout.
- [X] Set last error on Redis `-ERR`.
- [X] Close or invalidate client on protocol error.
- [X] Implement `command1`.
- [X] Implement `command2`.
- [X] Implement `command3`.
- [X] Implement `command4`.
- [X] Implement `command5`.
- [X] Implement `command_array` if current xlang array ergonomics are sufficient.
- [X] Implement `cluster_command_redirect_once_json(client, argv_json)`.
- [X] Implement `cluster_command_redirect_once_json(client, argv_json, timeout_ms)`.
- [X] Implement `cluster_command_redirect_retry_json(client, argv_json, max_redirects)`.
- [X] Implement `cluster_command_redirect_retry_json(client, argv_json, max_redirects, timeout_ms)`.
- [X] Implement `cluster_retryable_error(response_json)`.
- [X] Implement `cluster_command_slot_once_json(seed, slots_response_json, key, argv_json)`.
- [X] Implement `cluster_command_slot_once_json(seed, slots_response_json, key, argv_json, timeout_ms)`.
- [X] Implement `cluster_command_slot_retry_json(seed, slots_response_json, key, argv_json, max_redirects)`.
- [X] Implement `cluster_command_slot_retry_json(seed, slots_response_json, key, argv_json, max_redirects, timeout_ms)`.
- [X] Implement `cluster_command_key_once_json(seed, key, argv_json)`.
- [X] Implement `cluster_command_key_once_json(seed, key, argv_json, timeout_ms)`.
- [X] Implement `cluster_command_key_retry_json(seed, key, argv_json, max_redirects)`.
- [X] Implement `cluster_command_key_retry_json(seed, key, argv_json, max_redirects, timeout_ms)`.
- [X] Implement `cluster_command_slot_once_password_json(seed, slots_response_json, key, password, argv_json)`.
- [X] Implement `cluster_command_slot_retry_password_json(seed, slots_response_json, key, password, argv_json, max_redirects)`.
- [X] Implement `cluster_command_slot_once_auth_json(seed, slots_response_json, key, username, password, argv_json)`.
- [X] Implement `cluster_command_slot_retry_auth_json(seed, slots_response_json, key, username, password, argv_json, max_redirects)`.
- [X] Implement `cluster_command_key_once_password_json(seed, key, password, argv_json)`.
- [X] Implement `cluster_command_key_retry_password_json(seed, key, password, argv_json, max_redirects)`.
- [X] Implement `cluster_command_key_once_auth_json(seed, key, username, password, argv_json)`.
- [X] Implement `cluster_command_key_retry_auth_json(seed, key, username, password, argv_json, max_redirects)`.
- [X] Implement `cluster_slots_cache(slots_response_json)`.
- [X] Implement `cluster_slots_cache_json(cache)`.
- [X] Implement `cluster_slots_cache_valid(cache)`.
- [X] Implement `cluster_slots_cache_refresh(cache, seed)`.
- [X] Implement `cluster_command_cached_key_once_json(seed, cache, key, argv_json)`.
- [X] Implement `cluster_command_cached_key_once_json(seed, cache, key, argv_json, timeout_ms)`.
- [X] Implement `cluster_command_cached_key_retry_json(seed, cache, key, argv_json, max_redirects)`.
- [X] Implement `cluster_command_cached_key_retry_json(seed, cache, key, argv_json, max_redirects, timeout_ms)`.
- [X] Implement `cluster_command_cache_refresh_key_once_json(seed, cache, key, argv_json)`.
- [X] Implement `cluster_command_cache_refresh_key_once_json(seed, cache, key, argv_json, timeout_ms)`.
- [X] Implement `cluster_command_cache_refresh_key_retry_json(seed, cache, key, argv_json, max_redirects)`.
- [X] Implement `cluster_command_cache_refresh_key_retry_json(seed, cache, key, argv_json, max_redirects, timeout_ms)`.
- [X] Implement `cluster_command_cached_key_once_password_json(seed, cache, key, password, argv_json)`.
- [X] Implement `cluster_command_cached_key_retry_password_json(seed, cache, key, password, argv_json, max_redirects)`.
- [X] Implement `cluster_command_cached_key_once_auth_json(seed, cache, key, username, password, argv_json)`.
- [X] Implement `cluster_command_cached_key_retry_auth_json(seed, cache, key, username, password, argv_json, max_redirects)`.
- [X] Implement `cluster_command_cache_refresh_key_once_password_json(seed, cache, key, password, argv_json)`.
- [X] Implement `cluster_command_cache_refresh_key_retry_password_json(seed, cache, key, password, argv_json, max_redirects)`.
- [X] Implement `cluster_command_cache_refresh_key_once_auth_json(seed, cache, key, username, password, argv_json)`.
- [X] Implement `cluster_command_cache_refresh_key_retry_auth_json(seed, cache, key, username, password, argv_json, max_redirects)`.
- [X] Implement `cluster_get(seed, cache, key)`.
- [X] Implement `cluster_get_or(seed, cache, key, fallback)`.
- [X] Implement `cluster_set(seed, cache, key, value)`.
- [X] Implement `cluster_set_nx(seed, cache, key, value)`.
- [X] Implement `cluster_set_xx(seed, cache, key, value)`.
- [X] Implement `cluster_set_ex(seed, cache, key, value, seconds)`.
- [X] Implement `cluster_mget_json(seed, cache, keys)`.
- [X] Implement `cluster_mset(seed, cache, key_values)`.
- [X] Implement `cluster_append(seed, cache, key, value)`.
- [X] Implement `cluster_strlen(seed, cache, key)`.
- [X] Implement `cluster_del(seed, cache, key)`.
- [X] Implement `cluster_exists(seed, cache, key)`.
- [X] Implement `cluster_expire(seed, cache, key, seconds)`.
- [X] Implement `cluster_persist(seed, cache, key)`.
- [X] Implement `cluster_ttl(seed, cache, key)`.
- [X] Implement `cluster_pttl(seed, cache, key)`.
- [X] Implement `cluster_incr(seed, cache, key)`.
- [X] Implement `cluster_incr_by(seed, cache, key, amount)`.
- [X] Implement `cluster_decr(seed, cache, key)`.
- [X] Implement `cluster_decr_by(seed, cache, key, amount)`.
- [X] Implement `cluster_hget(seed, cache, key, field)`.
- [X] Implement `cluster_hget_or(seed, cache, key, field, fallback)`.
- [X] Implement `cluster_hset(seed, cache, key, field, value)`.
- [X] Implement `cluster_hdel(seed, cache, key, field)`.
- [X] Implement `cluster_hexists(seed, cache, key, field)`.
- [X] Implement `cluster_hlen(seed, cache, key)`.
- [X] Implement `cluster_hstrlen(seed, cache, key, field)`.
- [X] Implement `cluster_hgetall_json(seed, cache, key)`.
- [X] Implement `cluster_lpush(seed, cache, key, value)`.
- [X] Implement `cluster_rpush(seed, cache, key, value)`.
- [X] Implement `cluster_lpop(seed, cache, key)`.
- [X] Implement `cluster_lpop_or(seed, cache, key, fallback)`.
- [X] Implement `cluster_rpop(seed, cache, key)`.
- [X] Implement `cluster_rpop_or(seed, cache, key, fallback)`.
- [X] Implement `cluster_llen(seed, cache, key)`.
- [X] Implement `cluster_lrange_json(seed, cache, key, start, stop)`.
- [X] Implement `cluster_sadd(seed, cache, key, member)`.
- [X] Implement `cluster_srem(seed, cache, key, member)`.
- [X] Implement `cluster_sismember(seed, cache, key, member)`.
- [X] Implement `cluster_smembers_json(seed, cache, key)`.
- [X] Export `argv_json1` through `argv_json4` for typed raw command JSON construction.
- [X] Export `argv_json_list` for typed raw command JSON construction from arrays.
- [X] Implement `pipeline_json1` through `pipeline_json5`.
- [X] Implement `pipeline_json_array`.
- [X] Implement `command_pipeline_json`.
- [X] Implement `read_response_json`.
- [X] Decide whether fixed-arity command helpers are enough for V1 before requiring `command_array`. V1 exposes both fixed-arity helpers and list/JSON raw helpers.

Acceptance:

- [X] `command1(client, "PING")` returns simple JSON on a connected RESP socket.
- [X] Unknown command records Redis error text when a RESP error is received.
- [X] Timeout records a timeout error when a connected socket does not respond.
- [X] Raw command JSON builders escape nested strings through `std.json.encode`.
- [X] Pipeline JSON builders compose command JSON without hand-written nested JSON.
- [X] Pipeline returns multiple normalized responses from one TCP write.
- [X] One-shot cluster redirect helper follows one `MOVED` or `ASK` redirect for a raw JSON command.
- [X] Bounded cluster redirect helper follows a chain of `MOVED` or `ASK` redirects for a raw JSON command.
- [X] One-shot cluster slot helper routes one raw JSON command through a caller-provided `CLUSTER SLOTS` response.
- [X] Cluster slot retry helper follows bounded redirects after slot-owner command execution.
- [X] Cluster key helpers refresh `CLUSTER SLOTS` per call and route one raw JSON command by key.
- [X] Cluster password/auth raw command helpers authenticate temporary slot-owner and redirect-target connections before executing the target command.
- [X] Cluster slots cache helpers hold and refresh a caller-managed slots snapshot.
- [X] Cached cluster key helpers route one raw JSON command through a caller-managed slots cache.
- [X] Cache-refresh cluster key helpers refresh an empty caller-managed slots cache before routing.
- [X] Cache-refresh cluster key helpers refresh slots and retry once after retryable cluster errors.
- [X] Cluster password/auth raw command helpers cover per-call slots refresh, caller-managed cached slots, and cache-refresh-on-use routes.
- [X] Explicit cluster string wrappers route `GET`, fallback `GET`, `SET`, `SET NX`, `SET XX`, `SETEX`, same-slot `MGET`/`MSET`, `APPEND`, `STRLEN`, and `DEL` through a caller-managed slots cache.
- [X] Explicit cluster string wrappers route `EXISTS`, `EXPIRE`, `PERSIST`, `TTL`, and `PTTL` through a caller-managed slots cache.
- [X] Explicit cluster string wrappers route `INCR`, `INCRBY`, `DECR`, and `DECRBY` through a caller-managed slots cache.
- [X] Explicit cluster hash wrappers route `HGET`, fallback `HGET`, `HSET`, `HDEL`, `HEXISTS`, `HLEN`, `HSTRLEN`, and `HGETALL` through a caller-managed slots cache.
- [X] Explicit cluster list wrappers route `LPUSH`, `RPUSH`, `LPOP`, fallback `LPOP`, `RPOP`, fallback `RPOP`, `LLEN`, and `LRANGE` through a caller-managed slots cache.
- [X] Explicit cluster set wrappers route `SADD`, `SREM`, `SISMEMBER`, and `SMEMBERS` through a caller-managed slots cache.
- [X] Cluster string wrappers build raw command JSON with `std.json.encode` escaping.
- [X] Cluster hash wrappers build raw command JSON with `std.json.encode` escaping.
- [X] Cluster list wrappers build raw command JSON with `std.json.encode` escaping.
- [X] Cluster set wrappers build raw command JSON with `std.json.encode` escaping.
- [X] Retryable cluster error classification covers `READONLY`, `TRYAGAIN`, and `CLUSTERDOWN`.

## High-Level Wrappers: Server and Connection

- [X] Implement `ping(client)`.
- [X] Implement `ping(client, message)`.
- [X] Implement `auth(client, password)`.
- [X] Implement `auth(client, username, password)`.
- [X] Implement `hello(client, protocol)`.
- [X] Implement `hello_auth(client, protocol, username, password)`.
- [X] Implement `select(client, db)`.
- [X] Implement `dbsize(client)`.
- [X] Implement `flushdb(client)`.
- [X] Implement `sentinel_masters_json(client)`.
- [X] Implement `sentinel_master_json(client, name)`.
- [X] Implement `sentinel_get_master_addr_by_name_json(client, name)`.
- [X] Implement `sentinel_master_host(addr_response_json)`.
- [X] Implement `sentinel_master_port(addr_response_json)`.
- [X] Implement `sentinel_connect_master(sentinel, name)`.
- [X] Implement `sentinel_connect_master(sentinel, name, timeout_ms)`.
- [X] Implement `sentinel_command_master_once_json(sentinel, name, argv_json)`.
- [X] Implement `sentinel_command_master_once_json(sentinel, name, argv_json, timeout_ms)`.
- [X] Implement `sentinel_command_master_retry_json(sentinel, name, argv_json, max_attempts)`.
- [X] Implement `sentinel_command_master_retry_json(sentinel, name, argv_json, max_attempts, timeout_ms)`.
- [X] Implement `sentinel_command_master_once_password_json(sentinel, name, password, argv_json)`.
- [X] Implement `sentinel_command_master_once_password_json(sentinel, name, password, argv_json, timeout_ms)`.
- [X] Implement `sentinel_command_master_retry_password_json(sentinel, name, password, argv_json, max_attempts)`.
- [X] Implement `sentinel_command_master_retry_password_json(sentinel, name, password, argv_json, max_attempts, timeout_ms)`.
- [X] Implement `sentinel_command_master_once_auth_json(sentinel, name, username, password, argv_json)`.
- [X] Implement `sentinel_command_master_once_auth_json(sentinel, name, username, password, argv_json, timeout_ms)`.
- [X] Implement `sentinel_command_master_retry_auth_json(sentinel, name, username, password, argv_json, max_attempts)`.
- [X] Implement `sentinel_command_master_retry_auth_json(sentinel, name, username, password, argv_json, max_attempts, timeout_ms)`.
- [X] Implement `sentinel_ping(sentinel, name)`.
- [X] Implement `sentinel_ping(sentinel, name, message)`.
- [X] Implement `sentinel_select(sentinel, name, db)`.
- [X] Implement `sentinel_dbsize(sentinel, name)`.
- [X] Implement `sentinel_get(sentinel, name, key)`.
- [X] Implement `sentinel_get_or(sentinel, name, key, fallback)`.
- [X] Implement `sentinel_set(sentinel, name, key, value)`.
- [X] Implement `sentinel_set_nx(sentinel, name, key, value)`.
- [X] Implement `sentinel_set_xx(sentinel, name, key, value)`.
- [X] Implement `sentinel_set_ex(sentinel, name, key, value, seconds)`.
- [X] Implement `sentinel_mget_json(sentinel, name, keys)`.
- [X] Implement `sentinel_mset(sentinel, name, key_values)`.
- [X] Implement `sentinel_append(sentinel, name, key, value)`.
- [X] Implement `sentinel_strlen(sentinel, name, key)`.
- [X] Implement `sentinel_del(sentinel, name, key)`.
- [X] Implement `sentinel_exists(sentinel, name, key)`.
- [X] Implement `sentinel_expire(sentinel, name, key, seconds)`.
- [X] Implement `sentinel_persist(sentinel, name, key)`.
- [X] Implement `sentinel_ttl(sentinel, name, key)`.
- [X] Implement `sentinel_pttl(sentinel, name, key)`.
- [X] Implement `sentinel_incr(sentinel, name, key)`.
- [X] Implement `sentinel_incr_by(sentinel, name, key, amount)`.
- [X] Implement `sentinel_decr(sentinel, name, key)`.
- [X] Implement `sentinel_decr_by(sentinel, name, key, amount)`.
- [X] Implement `sentinel_hget(sentinel, name, key, field)`.
- [X] Implement `sentinel_hget_or(sentinel, name, key, field, fallback)`.
- [X] Implement `sentinel_hset(sentinel, name, key, field, value)`.
- [X] Implement `sentinel_hdel(sentinel, name, key, field)`.
- [X] Implement `sentinel_hexists(sentinel, name, key, field)`.
- [X] Implement `sentinel_hlen(sentinel, name, key)`.
- [X] Implement `sentinel_hstrlen(sentinel, name, key, field)`.
- [X] Implement `sentinel_hgetall_json(sentinel, name, key)`.
- [X] Implement `sentinel_lpush(sentinel, name, key, value)`.
- [X] Implement `sentinel_rpush(sentinel, name, key, value)`.
- [X] Implement `sentinel_lpop(sentinel, name, key)`.
- [X] Implement `sentinel_lpop_or(sentinel, name, key, fallback)`.
- [X] Implement `sentinel_rpop(sentinel, name, key)`.
- [X] Implement `sentinel_rpop_or(sentinel, name, key, fallback)`.
- [X] Implement `sentinel_llen(sentinel, name, key)`.
- [X] Implement `sentinel_lrange_json(sentinel, name, key, start, stop)`.
- [X] Implement `sentinel_sadd(sentinel, name, key, member)`.
- [X] Implement `sentinel_srem(sentinel, name, key, member)`.
- [X] Implement `sentinel_sismember(sentinel, name, key, member)`.
- [X] Implement `sentinel_smembers_json(sentinel, name, key)`.
- [X] Implement `sentinel_replicas_json(client, name)`.
- [X] Implement `sentinel_sentinels_json(client, name)`.
- [X] Implement `cluster_info_json(client)`.
- [X] Implement `cluster_nodes_json(client)`.
- [X] Implement `cluster_slots_json(client)`.
- [X] Implement `cluster_slots_node_host(slots_response_json, slot)`.
- [X] Implement `cluster_slots_node_port(slots_response_json, slot)`.
- [X] Implement `cluster_connect_slot(slots_response_json, slot)`.
- [X] Implement `cluster_connect_slot(slots_response_json, slot, timeout_ms)`.
- [X] Implement `cluster_keyslot(client, key)`.
- [X] Implement `cluster_myid(client)`.
- [X] Implement `cluster_redirect_type(response_json)`.
- [X] Implement `cluster_redirect_slot(response_json)`.
- [X] Implement `cluster_redirect_endpoint(response_json)`.
- [X] Implement `cluster_redirect_host(response_json)`.
- [X] Implement `cluster_redirect_port(response_json)`.
- [X] Implement `cluster_retryable_error(response_json)`.
- [X] Implement `cluster_connect_redirect(response_json)`.
- [X] Implement `cluster_connect_redirect(response_json, timeout_ms)`.
- [X] Implement `cluster_asking(client)`.
- [X] Confirm whether `flushdb` should be included in public V1 because it is destructive. Kept in public V1 with explicit README warning.

Acceptance:

- [X] `ping` returns `PONG` on a connected RESP socket.
- [X] `hello` and `hello_auth` return normalized raw JSON on a connected RESP socket.
- [X] `select` returns `true` for `+OK` on a connected RESP socket.
- [X] Sentinel discovery helpers return normalized raw JSON on a connected RESP socket.
- [X] Sentinel master address helpers parse host/port and can explicitly connect to the current master.
- [X] Sentinel one-shot current-master command helper executes a raw JSON command through a freshly discovered master.
- [X] Sentinel retry helper repeats current-master lookup and raw command execution for bounded attempts after error responses.
- [X] Sentinel password/auth raw command helpers authenticate the temporary current-master connection before executing the target command.
- [X] Sentinel connection/server wrappers execute `PING`, message `PING`, `SELECT`, and `DBSIZE` through the current-master lookup route.
- [X] Sentinel string wrappers execute `GET`, fallback `GET`, `SET`, `SET NX`, `SET XX`, `SETEX`, `MGET`, `MSET`, `APPEND`, `STRLEN`, `DEL`, `EXISTS`, `EXPIRE`, `PERSIST`, `TTL`, `PTTL`, `INCR`, `INCRBY`, `DECR`, and `DECRBY` through the current-master lookup route.
- [X] Sentinel hash wrappers execute `HGET`, fallback `HGET`, `HSET`, `HDEL`, `HEXISTS`, `HLEN`, `HSTRLEN`, and `HGETALL` through the current-master lookup route.
- [X] Sentinel list wrappers execute `LPUSH`, `RPUSH`, `LPOP`, fallback `LPOP`, `RPOP`, fallback `RPOP`, `LLEN`, and `LRANGE` through the current-master lookup route.
- [X] Sentinel set wrappers execute `SADD`, `SREM`, `SISMEMBER`, and `SMEMBERS` through the current-master lookup route.
- [X] Cluster discovery helpers return normalized raw JSON or mapped scalar values on a connected RESP socket.
- [X] Cluster slots helpers parse normalized `CLUSTER SLOTS` JSON and can explicitly connect to a slot owner.
- [X] Cluster redirect helpers parse `MOVED` and `ASK` error JSON into routing fields.
- [X] Cluster retryable error helper identifies `READONLY`, `TRYAGAIN`, and `CLUSTERDOWN`.
- [X] Cluster redirect connect helper can explicitly open a target node connection; `cluster_asking` maps `+OK`.
- [X] Destructive commands are clearly documented.

## High-Level Wrappers: Strings

- [X] Implement `get`.
- [X] Implement `get_or`.
- [X] Implement `exists`.
- [X] Implement `set`.
- [X] Implement `set_nx`.
- [X] Implement `set_xx`.
- [X] Implement `set_ex`.
- [X] Implement `mget_json`.
- [X] Implement `mset`.
- [X] Implement `append`.
- [X] Implement `strlen`.
- [X] Implement `del`.
- [X] Implement `expire`.
- [X] Implement `persist`.
- [X] Implement `ttl`.
- [X] Implement `pttl`.
- [X] Implement `incr`.
- [X] Implement `incr_by`.
- [X] Implement `decr`.
- [X] Implement `decr_by`.

Acceptance:

- [X] `set/set_nx/set_xx/get/del` wrapper response mapping is covered on a connected RESP socket.
- [X] `mset/mget_json` wrapper response mapping is covered on a connected RESP socket.
- [X] `append/strlen` wrapper response mapping is covered on a connected RESP socket.
- [X] Missing `get` returns empty string and `get_or` returns fallback on nil bulk.
- [X] `exists` maps integer response to bool.
- [X] `persist` maps integer response to bool and `pttl` maps integer response to milliseconds.
- [X] `incr_by` and `decr_by` map integer response to the updated counter value.

## High-Level Wrappers: Hashes

- [X] Implement `hget`.
- [X] Implement `hget_or`.
- [X] Implement `hset`.
- [X] Implement `hdel`.
- [X] Implement `hexists`.
- [X] Implement `hlen`.
- [X] Implement `hstrlen`.
- [X] Implement `hgetall_json`.

Acceptance:

- [X] `hset/hget/hget_or/hdel/hexists/hlen/hstrlen` wrapper response mapping is covered on a connected RESP socket.
- [X] `hlen` returns the integer field count.
- [X] `hstrlen` returns the string length for a hash field.
- [X] `hgetall_json` returns normalized RESP array JSON as documented.

## High-Level Wrappers: Lists

- [X] Implement `lpush`.
- [X] Implement `rpush`.
- [X] Implement `lpop`.
- [X] Implement `lpop_or`.
- [X] Implement `rpop`.
- [X] Implement `rpop_or`.
- [X] Implement `llen`.
- [X] Implement `lrange_json`.

Acceptance:

- [X] `lpush/rpush/llen/lrange/lpop/lpop_or/rpop/rpop_or` wrapper response mapping is covered on a connected RESP socket.
- [X] `lrange_json` returns normalized RESP array JSON.

## High-Level Wrappers: Sets

- [X] Implement `sadd`.
- [X] Implement `srem`.
- [X] Implement `sismember`.
- [X] Implement `smembers_json`.

Acceptance:

- [X] `sadd/sismember/smembers/srem` wrapper response mapping is covered on a connected RESP socket.
- [X] `smembers_json` returns normalized RESP array JSON.

## Offline Examples and Tests

- [X] Create `demo5/release/examples/redis/redis_offline_codec.xl`.
- [X] Test encoder for `PING`.
- [X] Test encoder for `SET`.
- [X] Test raw command JSON builders.
- [X] Test pipeline JSON builders.
- [X] Test JSON escaping.
- [X] Test normalized response extractors.
- [X] Test simple string parser.
- [X] Test error parser.
- [X] Test integer parser.
- [X] Test bulk parser.
- [X] Test nil bulk parser.
- [X] Test array parser.
- [X] Test split chunks.
- [X] Test malformed response handling.
- [X] Test common RESP3 response parsing.
- [X] Test failed connect and closed client command handling.
- [X] Test connected command success, Redis error, high-level `ping`/`select`, and timeout with a loopback RESP socket.
- [X] Test pipeline encoding and multi-response parsing with a loopback RESP socket.
- [X] Test pool acquire/release/close with a loopback RESP socket.

Acceptance:

- [X] Offline example returns `0` without Redis.
- [X] Offline example uses stable non-zero failure codes.

## Real Smoke

- [X] Create `demo5/release/examples/redis/redis_basic.xl`.
- [X] Read `XL_REDIS_HOST`, `XL_REDIS_PORT`, `XL_REDIS_PASSWORD`, and `XL_REDIS_DB` where possible.
- [X] Require `XL_REDIS_REAL=1` in wrapper scripts.
- [X] Create `dev/extlibs/redis/run_x_redis_smoke.bat`.
- [X] Create `dev/extlibs/redis/run_x_redis_smoke.sh`.
- [X] Create optional fake RESP2 smoke for local end-to-end validation without Redis.
- [X] Create all-checks wrapper scripts for default x.redis validation without Redis.
- [X] Create export metadata parity check for `src/redis.xl` and `package.json`.
- [X] Keep fake smoke timeout high enough for expanded `redis_basic.xl` coverage and kill the child process on timeout.
- [X] Use unique key prefix.
- [X] Clean up keys before exit.
- [X] Real smoke uses a unified cleanup path after command assertions.
- [X] Test `PING`. Verified with temporary local Redis 3.0.504.
- [X] Test `SET/SET_NX/SET_XX/GET/DEL/EXISTS`. Verified with temporary local Redis 3.0.504 and fake RESP2 smoke.
- [X] Test `SETEX/TTL/PTTL/EXPIRE/PERSIST`. Verified with temporary local Redis 3.0.504 and fake RESP2 smoke.
- [X] Test `HSET/HGET/HGET_OR/HEXISTS/HLEN/HSTRLEN/HGETALL/HDEL`. Verified with temporary local Redis 3.0.504 and fake RESP2 smoke.
- [X] Test `LPUSH/RPUSH/LLEN/LRANGE/LPOP/LPOP_OR/RPOP/RPOP_OR`. Verified with temporary local Redis 3.0.504 and fake RESP2 smoke.
- [X] Test `SADD/SISMEMBER/SMEMBERS/SREM`. Verified with temporary local Redis 3.0.504.
- [X] Test `command_pipeline_json`. Verified with temporary local Redis 3.0.504.
- [X] Test `subscribe/publish/read_response_json/unsubscribe`. Verified with temporary local Redis 3.0.504.
- [X] Test `pool_acquire/pool_release/pool_close`. Verified with temporary local Redis 3.0.504.

Acceptance:

- [X] Smoke script skips clearly when `XL_REDIS_REAL` is not `1`.
- [X] Smoke script propagates `RunResult exitCode` from `redis_basic.xl`.
- [X] Smoke does not write unprefixed keys.
- [X] Smoke cleans up keys on the success path.
- [X] Fake smoke can run `redis_basic.xl` against a local RESP2 test server.
- [X] All-checks wrapper runs build, offline example, default regression selector, and fake smoke.
- [X] All-checks wrapper verifies source exports match package metadata exports.
- [X] All-checks wrapper verified on Windows through `dev\extlibs\redis\run_x_redis_all_checks.bat`.
- [X] POSIX all-checks wrapper invokes fake smoke through `sh` so it does not depend on executable file mode.
- [X] Fake smoke covers one-shot cluster `MOVED` and `ASK` redirect retry with a second local RESP server.
- [X] Fake smoke covers bounded cluster redirect-chain retry with three local RESP servers.
- [X] Fake smoke covers one-shot slot-owner command routing through a provided `CLUSTER SLOTS` response.
- [X] Fake smoke covers stale slot-owner command routing followed by bounded redirect retry.
- [X] Fake smoke covers per-call `CLUSTER SLOTS` refresh key routing.
- [X] Fake smoke covers cluster password/auth raw command helpers for slot-owner and redirect-target routes.
- [X] Fake smoke covers caller-managed cluster slots cache refresh and cached key routing.
- [X] Fake smoke covers cache-refresh key routing from an empty caller-managed slots cache.
- [X] Fake smoke covers cluster password/auth raw command helpers for cached slots and cache-refresh-on-use routes.
- [X] Fake smoke covers cache refresh plus retry after a simulated cluster `READONLY` error.
- [X] Fake smoke covers explicit cluster string wrappers.
- [X] Fake smoke covers explicit cluster `APPEND` and `STRLEN` wrappers.
- [X] Fake smoke covers explicit cluster `SET NX` and `SET XX` wrappers.
- [X] Fake smoke covers explicit cluster `GET` fallback wrapper.
- [X] Fake smoke covers explicit cluster `EXISTS`, `EXPIRE`, and `TTL` wrappers.
- [X] Fake smoke covers explicit cluster `PERSIST` and `PTTL` wrappers.
- [X] Fake smoke covers explicit cluster `SETEX` wrapper.
- [X] Fake smoke covers explicit cluster same-slot `MGET` and `MSET` wrappers.
- [X] Fake smoke covers explicit cluster `INCR` and `DECR` wrappers.
- [X] Fake smoke covers explicit cluster `INCRBY` and `DECRBY` wrappers.
- [X] Fake smoke covers explicit cluster hash wrappers including `HLEN` and `HSTRLEN`.
- [X] Fake smoke covers explicit cluster `HGET` fallback wrapper.
- [X] Fake smoke covers explicit cluster list wrappers.
- [X] Fake smoke covers explicit cluster `LPOP`/`RPOP` fallback wrappers.
- [X] Fake smoke covers explicit cluster set wrappers.
- [X] Fake smoke covers cluster string wrapper values containing quotes and backslashes.
- [X] Fake smoke covers one-shot Sentinel current-master command execution with a second local RESP server.
- [X] Fake smoke covers bounded Sentinel current-master retry after a simulated failover error.
- [X] Fake smoke covers Sentinel password/auth current-master raw command helpers.
- [X] Fake smoke covers explicit Sentinel current-master `PING`, message `PING`, `SELECT`, and `DBSIZE` wrappers.
- [X] Fake smoke covers explicit Sentinel current-master string wrappers.
- [X] Fake smoke covers explicit Sentinel current-master `MGET` and `MSET` wrappers.
- [X] Fake smoke covers explicit Sentinel current-master `APPEND` and `STRLEN` wrappers.
- [X] Fake smoke covers explicit Sentinel current-master `SET NX` and `SET XX` wrappers.
- [X] Fake smoke covers explicit Sentinel current-master `GET` fallback wrapper.
- [X] Fake smoke covers explicit Sentinel current-master TTL and counter wrappers.
- [X] Fake smoke covers explicit Sentinel current-master `INCRBY` and `DECRBY` wrappers.
- [X] Fake smoke covers explicit Sentinel current-master `PERSIST` and `PTTL` wrappers.
- [X] Fake smoke covers explicit Sentinel current-master hash wrappers including `HLEN` and `HSTRLEN`.
- [X] Fake smoke covers explicit Sentinel current-master `HGET` fallback wrapper.
- [X] Fake smoke covers explicit Sentinel current-master list wrappers.
- [X] Fake smoke covers explicit Sentinel current-master `LPOP`/`RPOP` fallback wrappers.
- [X] Fake smoke covers explicit Sentinel current-master set wrappers.

## Documentation

- [X] Document package installation location.
- [X] Document import syntax.
- [X] Document V1 transport limitations.
- [X] Document supported Redis commands.
- [X] Document raw command JSON response format.
- [X] Document real smoke environment variables.
- [X] Document destructive command warnings.
- [X] Document future native backend path.

Acceptance:

- [X] A user can run the offline example from README instructions.
- [X] A user can run the real smoke against local Redis from README instructions.

## Regression Integration

- [X] Decide whether `redis_offline_codec.xl` belongs in default regression.
- [X] Add default regression entry only for offline example.
- [X] Keep real smoke out of default regression.
- [X] Confirm the current test runner mechanism for adding new example-level cases.

Acceptance:

- [X] Default regression remains independent of Redis installed state.
- [X] Any real Redis test is opt-in.

## Future Work

- [X] Create blocked handoff spec for remaining `[!]` items in `dev/extlibs/redis/x.redis-blocked-handoff-spec.md`.
- [X] Create compact V1 status handoff in `dev/extlibs/redis/x.redis-v1-status.md`.
- [X] Create V1 changelog in `dev/extlibs/redis/x.redis-v1-changelog.md`.
- [!] Native binary-safe transport. Current `std.net.tcp.send` uses string length semantics and `std.net.tcp.recv` returns `string`; `x.redis` now exposes `transport_binary_safe() == false` and `transport_limits_json()` so callers can detect the limitation. Completing this requires a byte-buffer transport API with send/recv lengths preserved across NUL bytes; see the blocked handoff spec.
- [!] TLS support. Current `std.net.tls` exposes TLS metadata/config helpers but no TLS socket connect/send/recv/close API; `x.redis` now exposes `transport_tls() == false` and `transport_limits_json()` so callers can detect the limitation. Completing this requires a TLS socket transport or a separate Redis transport library that owns TLS I/O; see the blocked handoff spec.
- [X] Pipeline API. Implemented as `command_pipeline_json`, `resp_encode_pipeline_json`, `argv_json*`, `argv_json_list`, and `pipeline_json*`.
- [X] Pub/sub API. Basic synchronous helpers implemented; event-loop integration remains out of scope.
- [X] Connection pool. Implemented as a fixed four-slot idle socket pool.
- [X] Sentinel support. Basic discovery, explicit master connect, one-shot current-master command, password/auth current-master raw command helpers, bounded retry helpers, explicit current-master connection/server wrappers, explicit current-master string/hash/list/set wrappers, `MGET`/`MSET`, `APPEND`/`STRLEN`, `SET NX`/`SET XX`, `PERSIST`/`PTTL`, `INCRBY`/`DECRBY`, `HLEN`/`HSTRLEN`, `GET`/`HGET`/`LPOP`/`RPOP` fallback wrappers, and capability reporting are implemented.
- [!] Sentinel background monitoring and failover-aware persistent connection management remain out of scope for the V1 source package; see the blocked handoff spec.
- [X] Cluster support. Basic discovery, explicit `CLUSTER SLOTS` endpoint lookup/connect, password/auth raw command helpers for temporary slot-owner and redirect-target connections, caller-managed long-lived slots cache, cached key routing, explicit cache-refresh-on-use key routing, explicit cluster string/hash/list/set wrappers, same-slot `MGET`/`MSET`, `APPEND`/`STRLEN`, `SET NX`/`SET XX`, `PERSIST`/`PTTL`, `INCRBY`/`DECRBY`, `HLEN`/`HSTRLEN`, `GET`/`HGET`/`LPOP`/`RPOP` fallback wrappers, stale-slot bounded redirect recovery, retryable cluster error classification and bounded recovery, per-call slots refresh key routing, `MOVED`/`ASK` parsing, explicit redirect connect, `ASKING`, one-shot raw command redirect retry, bounded redirect-chain retry, and capability reporting are implemented.
- [!] Transparent Cluster routing for ordinary commands remains out of scope for the V1 source package; see the blocked handoff spec.
- [X] RESP3 parser support for common response types.
- [X] Explicit RESP protocol negotiation helpers. Implemented as `hello` and `hello_auth`; default connections still use RESP2 until callers opt in.
- [X] Typed response extractors for normalized RESP2/RESP3 JSON, including array, set, indexed map, and scalar-key map helpers.
