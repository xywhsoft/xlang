# xdistdb - Distributed Key-Value Database (5-star verification project)

A complete distributed KV database simulation implemented in pure xlang, covering:
consistent hashing, sharding, Raft consensus, replication, multiple data types,
query routing, cross-shard transactions, and a Redis-like + HTTP API.

## Layout

```
xdistdb/
    xdistdb.xl       Main source (~5000 lines)
    xdistdb.exe      Built executable (xl build)
    README.md        This file
```

## Run

```bat
cd /d D:\GIT\x-lang\demo6\release

REM Run via interpreter
xl run projects\dev\xdistdb\xdistdb.xl -- test
xl run projects\dev\xdistdb\xdistdb.xl -- cluster init 5
xl run projects\dev\xdistdb\xdistdb.xl -- set mykey "hello"
xl run projects\dev\xdistdb\xdistdb.xl -- get mykey

REM Build standalone exe
xl build projects\dev\xdistdb\xdistdb.xl -o projects\dev\xdistdb\xdistdb.exe
projects\dev\xdistdb\xdistdb.exe test
projects\dev\xdistdb\xdistdb.exe cluster init 5
projects\dev\xdistdb\xdistdb.exe set mykey "hello"
```

## Feature coverage

1. **Consistent hashing** - 150 vnodes per physical node, 2^32 hash space,
   join/leave -> migration range computation, ASCII ring visualization.
2. **Sharding** - 64 fixed logical shards, `key -> hash -> shard -> primary`
   mapping, split/merge metadata.
3. **Raft consensus** - per-shard full state machine: currentTerm / votedFor /
   log / commitIndex / lastApplied / snapshot. Leader election (RequestVote
   majority), AppendEntries replication, snapshot-based log compaction,
   add/remove server membership changes.
4. **Replication** - leader -> followers sync, linearizable reads via
   leader-read + lease freshness, automatic failover on leader loss.
5. **Data model** - string / list / hash / set / zset with full op set
   (GET/SET/DEL/INCR/EXPIRE/TTL/PERSIST, LPUSH/RPUSH/LPOP/RPOP/LRANGE/LLEN,
   HSET/HGET/HGETALL/HDEL/HLEN, SADD/SREM/SMEMBERS/SISMEMBER/SCARD,
   ZADD/ZREM/ZRANGE/ZSCORE/ZCARD). TTL with lazy + active expiration.
6. **Query routing** - client -> coordinator -> shard via consistent hash,
   scatter-gather for KEYS/COUNT, cross-shard 2PC (two-phase commit) for
   multi-key transactions.
7. **Client protocol** - Redis-like text protocol (SET/GET/DEL/INCR/...,
   CLUSTER INFO/NODES/SHARDS/RING/HEALTH), HTTP API with JSON responses.
8. **Operations** - cluster info / nodes / shards / slots / health /
   migrations / distribution, heartbeat + health checks, migration progress.

## CLI

```
xdistdb cluster init [nodes=5]    initialize cluster
xdistdb cluster add [port]        add node (triggers migration)
xdistdb cluster remove <nodeId>   remove node (triggers migration)
xdistdb cluster status            full status (info + nodes + shards + slots + dist)
xdistdb cluster ring              consistent hash ring visualization
xdistdb cluster nodes/shards/slots/health/migrations/distribution
xdistdb set/get/del/exists/type/incr/expire/ttl/persist <key> ...
xdistdb lpush/rpush/lpop/rpop/lrange/llen <key> ...
xdistdb hset/hget/hgetall/hdel/hlen <key> ...
xdistdb sadd/srem/smembers/sismember/scard <key> ...
xdistdb zadd/zrange/zscore/zcard/zrem <key> ...
xdistdb keys <pattern>            scatter-gather KEYS
xdistdb count                     total key count
xdistdb fail <nodeId>             simulate node failure
xdistdb recover <nodeId>          recover failed node
xdistdb raft <shardId>            raft state of a shard
xdistdb eval <line>               Redis-like protocol line
xdistdb tx <c1> | <c2> | ...      cross-shard 2PC
xdistdb export/import <file>      state serialization
xdistdb reset                     clear all state
xdistdb test                      built-in test suite (303 assertions)
xdistdb bench [ops=1000]          write+read+balance benchmark
xdistdb serve [port=18300]        HTTP API server
xdistdb help                      help
```

## HTTP API (serve mode)

```
GET  /                       welcome page
GET  /key/<key>              get value (JSON)
POST /key/<key>  body=value  set value (JSON)
DELETE /key/<key>            delete value (JSON)
GET  /cluster/info           cluster info (text)
GET  /cluster/nodes          node list (text)
GET  /cluster/shards         shard list (text)
GET  /cluster/health         health check (text)
GET  /cluster/ring           hash ring visualization (text)
GET  /cluster/distribution   key distribution (text)
GET  /cluster/migrations     migration progress (text)
GET  /keys?pattern=*         list keys (JSON)
GET  /count                  total key count (JSON)
GET  /raft/<shardId>         raft state (text)
POST /cmd  body=<line>       Redis-like command (JSON)
```

## Test scenarios

The built-in `test` command runs 25 test modules with 303 assertions:
- cluster init (5 nodes / 64 shards / 750 vnodes)
- hash ring + key routing determinism
- 1000-key distribution balance (CV < 0.5)
- all 5 data types (string/list/hash/set/zset)
- TTL lazy + active expiration
- Raft log replication + snapshot + membership
- replication + linearizable read
- single + multi-node failover (1 of 5, 2 of 5)
- node join/leave + migration ranges
- scatter-gather KEYS/COUNT
- cross-shard 2PC commit/abort
- Redis-like protocol
- shard split/merge thresholds
- end-to-end (init -> 1000 keys -> fail -> verify -> recover -> verify)
- export/import + save/load persistence
- workload statistics tracking
- cluster status formatting

## Design notes

- **State persistence**: `xdistdb.state.json` in script/exe directory. Each CLI
  invocation: `load -> mutate -> save`. Single-process simulation of a cluster
  (no real network).
- **xlang semantics**: dict index access returns a by-value copy. To mutate a
  nested value, read into a local, mutate, write back via `putXxx` helpers.
  Same for arrays passed to sort functions - they return sorted copies.
- **Reserved word**: `out` is reserved in xlang (output parameter modifier).
  Avoid as identifier; use `res` or `result` instead.
