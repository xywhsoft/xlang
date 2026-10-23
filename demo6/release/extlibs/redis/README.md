# redis

`redis` 提供 RESP2 编解码和基于标准库 `TcpStream` 的单命令客户端。
原生层不管理连接；C 可以直接使用长度感知的编解码接口，XL 再组合
`tcp.connect`、`send`、`drain`、`recv` 完成命令调用。不包含连接池、
自动重连、订阅模式或事务编排。

```xlang
import redis

string request = redis.encode(["SET", "key", "value"])
var reply = redis.decode("$5\r\nhello\r\n")
assert reply["value"]["type"] == "bulk"
assert reply["value"]["value"] == "hello"
```

`encode` 要求非空字符串数组，返回文本 RESP；`encodeBytes` 接受字符串或
`bytes` 参数并返回拥有型 `Buffer`，可发送非 UTF-8 的二进制键和值。
`command` 使用原始字节路径，参数可混用字符串和 `bytes`；`Buffer` 参数
先调用 `toBytes()`。`decode` 解析第一条完整回复，返回
`{type: "reply", value: {type, value}, rest}`，其中 `rest` 保留后续字节。
回复类型为 `simple`、`error`、`integer`、`bulk`、`array` 或 `null`；
数组元素仍是带 `type/value` 的对象。有效 UTF-8 的 bulk 值为
`string`，非 UTF-8 的 bulk 值为 `bytes`；`rest` 也按同一规则返回。
可使用 `decodeBytes(Buffer)`
和 `isCompleteBytes(Buffer)` 解析原始字节，避免文本路径的编码限制。
`release/examples/extlibs/redis.xl` 与 `redis_c.c` 的成对范例先检查一条
未收齐的二进制 bulk 回复，再解析含 `0xff` 的完整回复并逐字节核对，
无需连接真实 Redis 服务。
`isComplete` 对不完整回复返回
`false`，对畸形回复抛 `ValueError`。编码和解析均按**字节长度**处理，
字符串中的 `\0` 不会截断数据。单帧上限 64 MiB，数组最多十万项，
整条回复累计最多十万个值节点，嵌套深度最多 64 层。
含非 UTF-8 字节的 RESP error 负载在原始解码结果中仍为 `bytes`，
不会在解码层丢失字节；`command` 才将完整错误回复转换为包级异常。

`command(stream, args, timeoutMs=5000, maxReplyBytes=67108864)` 在已有
`TcpStream` 上发送**一条**命令并通过 `recvBytes` 读取**一条**回复。
超时以本次调用开始时刻计时，不会因服务端持续滴送小片段而延长；
`timeoutMs` 必须为 1..2147483647，回复上限必须为 1..67108864 字节。
参数在发送前校验，失败不消耗连接。底层接收超时按
总期限归一为 `redis: command timed out`，其他接收错误保留错误描述。
Redis error 回复抛 `ValueError`，同次读取出现额外回复时
也会拒绝，避免默默丢失流水线数据。流水线调用应由使用者组合
`encode`/`decode` 与 `rest` 管理。`get` 返回 `var`，不存在的键返回
`null`，不会与空字符串混淆。`set`、`del`、`incr`、`exists` 是便捷函数。
`command` 借用调用方的连接：参数编码失败时尚未发送数据，连接不变；
完整的 Redis error 回复已消费完毕，连接可继续使用。发送、超时、
畸形回复、超限或多余回复使请求与回复无法可靠对应时，`command`
调用 `stream.abort()`，此连接不得再用于下一条命令，调用方应显式重连。
这不改变调用方对 `TcpStream` 对象的所有权。
`command` 收包时使用增量 RESP 帧检查器：每次只扫描新追加的字节，
不会因大数组被拆成许多小 TCP 片段而反复解析旧前缀；完整回复仍由原解码器
构造一次结果。C 可使用 `xr_frame_create/check/destroy` 实现相同检查；
`check` 的缓冲内容必须只追加，地址可变，返回 `false` 且无 XRT 错误表示
尚未收齐，格式错误则报告 XRT 错误。原有 `isComplete`/`isCompleteBytes`
仍是独立输入的无状态检查，不持有或缓存缓冲内容。
本地回环还以含非 UTF-8 字节的完整 error 帧验证：`command` 抛出带
`redis:` 前缀的 `ValueError`，同一连接随后执行 `PING` 仍能成功。

C 接口见 `include/redis_native.h`，输入 `xvalue*` 仅借用；
`xr_encode_bytes` 返回独立拥有的 XRT `xbuffer*`，C 调用方以
`xrtBufferDestroy` 释放；文本 `xr_encode` 仍只接受字符串。
`xr_decode_raw` / `xr_is_complete_raw` 直接接受指针与字节长度。
成功返回的
`xvalue*` 由调用方以 `xrtValueRelease` 释放。原生错误返回 `null`
值并保留 XRT 结构化错误。所有编解码结果独立拥有，不使用静态环形缓冲。

包级验证：

- 在 `release` 目录运行 `xl run examples/extlibs/redis.xl` 或
  `xl run examples/extlibs/redis_c.c`，输出一致，均不依赖外部服务。
  C 范例直接包含原生实现；从 `demo6` 根目录调用时也可使用
  `tools/run_extlib_c.ps1 -Package redis -Source release/examples/extlibs/redis_c.c`。
- `xl run extlibs/redis/test/boundaries.xl`
- `tools/smoke_extlib_catalog.ps1 -Package redis` 会发现包清单登记的无设备
  边界测试和中央扩展库用例；两者均按当前非空命令合同验证。回环、真服务与
  压力测试由聚焦门禁单独运行。
- `xl run extlibs/redis/test/command_loopback.xl` 在本机随机端口测试
  `TcpStream` 命令往返、分段回复、错误回复，以及持续滴送不完整回复时的
  整次调用超时；还检查自定义短超时、回复字节限额与参数校验前置。
  发送前的分配故障须抛 `MemoryError`，且不消耗连接；清除故障后同一连接
  可正常执行命令。
  畸形回复、超限和超时后验证旧流不再可发送。
- `test/check.ps1` 是不依赖真实服务的 Windows 包级门禁；普通模式包含
  C 边界/分配故障/线程、XL 边界与分配故障、TCP 回环和双语范例对照，
  并构建运行核心及完整范例 AOT，将后者输出与 C 范例对照。
  `-SkipAot` 只运行离线 C/JIT 快测，不表示发布签收。`-Stress` 另跑
  `test/reuse_loopback.xl` 的十万次同连接请求（每 17 次有一个 Redis
  error 回复）；2026-10-13 本机 500、10,000、100,000 次均通过。
  这是协议与连接复用探针，不替代真实 Redis 的长时验收。
- 包内 C/XLang 与中央 XLang 用例均固定空命令返回范围错误，随后 `PING` 编码恢复正常；
  不增加空命令兼容写法。
- 用 `tools/run_extlib_c.ps1` 运行 `test/test_redis.c`、
  `test/test_redis_alloc.c` 和 `test/test_redis_threads.c`；故障测试逐点拦截
  文本/二进制命令编码及嵌套回复解码的分配，要求失败保留内存错误类型，
  清除故障后生成的文本 RESP 与固定字节向量一致。
  `sh release/extlibs/redis/test/check.sh` 在独立 Linux 进程中运行这三组
  C 测试并启用 ASan/UBSan 泄漏检查，还运行中央及包内 XLang 边界、500 次本地
  TCP 回环复用及 C/XLang 范例逐字节对照；默认无需 Redis 服务。
  2026-10-16 Linux 完整离线门禁通过。
  `test/allocations.xl` 对文本和二进制编码/解码分别注入前 32 个分配故障点，
  要求触发时抛 `MemoryError`、清除故障后仍能完成四种操作；它同时进入
  Windows/Linux 包级门禁和包清单的 `coverage.tests`。
  2026-10-17 文本 `xr_encode` 补逐点分配失败与恢复扫描；Windows 完整
  离线包门禁及 Linux ASan/UBSan 离线包门禁通过，未改变公开接口。
  2026-10-18 在临时 Redis 8.0.5 上重跑 `REDIS_LIVE=1` 联合门禁：
  C ASan/UBSan、XLang 真服务和双语离线范例全部通过；另单独运行四连接
  各 10,000 轮并发用例通过。测试实例已关闭。这是当前构建的短时高请求量
  验证，不替代长时稳定性或完整 AOT 验收。
  2026-10-22 增量帧扫描器接入后重跑包内 `check_live.sh`：临时 Redis 上的
  C ASan/UBSan、XLang 真服务、错误后复用、重连、四连接并发（各 50 轮）和
  双语范例通过；服务由脚本清理。新增扫描器的真实网络主路径得到复验，
  完整 AOT 和长时稳定性仍未据此签收。
  同日再以 `REDIS_STRESS=1` 运行自管理临时 Redis 8.0.5 门禁：四连接各
  10,000 轮与 C ASan/UBSan、真实服务错误/重连及双语范例同次通过；退出后
  未留下 Redis 进程。该高请求量短时验证仍不代替长时运行或完整 AOT。
- WSL/Linux 已安装 Redis 并监听 `127.0.0.1:6379` 时，可从 `demo6`
  运行 `REDIS_LIVE=1 bash release/extlibs/redis/test/check.sh`。该可选分支
  以 ASan/UBSan 跑 C 真服务用例，再跑 XLang 单连接与四连接并发用例；
  默认不连接服务。2026-10-15 在 Redis 8.0.5 上三组均通过。
- WSL/Linux 已安装 `redis-server` 和 `redis-cli` 时，可运行
  `bash release/extlibs/redis/test/check_live.sh`：它选择本次临时端口，
  只启动绑定 `127.0.0.1`、关闭持久化的实例，并以 Redis 报告的进程 ID
  核对服务归属。已有 Redis 不会被连接或关闭；成功或失败均清理本次服务。
  真服务 C 示例和 C 边界先于 XLang 检查运行，故语言编译失败时仍可明确
  区分原生接口是否已经通过；整个脚本仍返回失败，不作降级验收。
- `examples/extlibs/redis_live.xl` 是可选的真实 Redis 示例，要求
  默认 `127.0.0.1:6379` 运行 Redis；设 `REDIS_LIVE_PORT` 可指定其他端口。
  `examples/extlibs/redis_live_c.c` 用 XRT socket
  与原生 RESP API 完成相同的 PING/SET/GET/EXISTS/INCR/DEL；
  同时验证键存在与删除后的不存在。C 接收路径使用增量帧检查器，分片回复
  不会重复扫描已接收内容。两份示例均在结束前删除演示键。缺服务时明确
  失败，不计入离线门禁。`check_live.sh` 在自管理临时
  Redis 上分别运行 GCC+ASan C、`xl run` C 与 XLang 示例并逐字节对照输出。
- `xl run extlibs/redis/test/live_server.xl` 是显式真实服务门禁：检查
  PING、包含 `00 FF` 的二进制键和值、缺失键、数组回复、错误后连接复用、
  服务端 `QUIT` 主动关闭后的失败及重连。2026-10-13 使用本机临时 Redis 8.0.5
  验证通过，服务已在测试后关闭；不依赖或发布这份测试二进制。
- `xl run extlibs/redis/test/concurrent_server.xl` 是可选的真服务并发门禁：
  四个任务各持一条连接，每条连接进行 50 轮 SET/GET/DEL，覆盖带 NUL 的
  值和错误回复后的复用。`-- 10000` 将每条连接增加到 10000 轮；测试键
  含本次运行的单调时间戳，失败遗留键也会在 10 秒后过期。不承诺同一
  `TcpStream` 上并发发送命令。Linux 可用 `REDIS_LIVE=1 REDIS_STRESS=1`
  运行 `test/check.sh`，同时保留 C ASan/UBSan 与双语真服务门禁。
- `tools/run_extlib_c.ps1 -Package redis -Source release/extlibs/redis/test/live_server_c.c`
  是对应的 C 真服务用例，使用 XRT socket 和 RESP 编解码 API，覆盖
  PING、包含 `00 FF` 的二进制键和值、数组回复、错误回复与清理。另以包内
  `test/live_runtime.c` 独立编译，在 Windows 严格 GCC 和 Linux
  ASan/UBSan 下连接 Redis 8.0.5 通过；这个运行时文件仅供 C 测试，
  不进入包清单。

XL AOT 当前因主线缺少 `xl6ValueDataSnapshotV1` 而失败，见
`demo6/dev/extlib_blockers/text_snapshot_link.md`。新增无输出
`test/aot_core.xl` 只调用 `isComplete`，JIT 通过，但 AOT 同样
缺该符号。包清单现声明与 `test/live_runtime.c` 一致的四个 XRT
模块；该裁剪运行时已独立编译并通过 C 边界，但最终 AOT 体积与运行
仍须待共享符号修复后验证。
2026-10-24 在当前 WSL 发布物上复跑 `check_live.sh`：C 边界、分配、线程
和真服务调用均通过 ASan/UBSan。单独执行
`./release/xl check test/cases/extlib_redis.xl` 也在包源码
`src/redis.xl:3` 报未知 `cstring`，
同次还报告未知 `cbytes`、`NetStatus` 与 `TcpWait`。这是该 Linux 发布版
编译器与当前包/标准库声明不一致的可独立复现阻断；同步 Linux 发布物后
重跑完整 C/XLang 真服务门禁，不在包内增加旧签名回退，也不把 C 通过
记作 XLang 通过。
真实 Redis 单连接成功/错误/主动断线后显式重连及四连接短轮次并发路径已验证，C 原生宿主也
经过真服务门禁；尚未覆盖长时运行、
自动重试策略（当前 API 不承诺自动重连）以及完整 AOT，因此暂不签收发布验收。

2026-10-20：Windows 包级默认门禁加入核心与完整带输出范例的 AOT
构建、运行和 C 输出对照；独占构建目录在退出时清理。`-SkipAot` 保留
C/JIT 快测。当前构建的共享 AOT 链接缺符号仍是发布阻断。
2026-10-16 在临时本机 Redis 上，Windows `xl.exe` 和 Linux `release/xl`
均完成四连接各 10000 轮真服务压力测试，总计 120000 条 SET/GET/DEL
命令，并周期性验证错误回复后的连接复用；Linux 包门禁的 C sanitizer
也通过。这是短时高请求量验证，不等同于数小时持续运行。

2026-10-17：TCP 回环中对畸形回复的断言从“任意 `ValueError`”收紧为
`RESP reply is malformed`，避免断线/超时被误当作解析成功拒绝。
Linux 离线门禁也纳入同一组分段、错误与截止时间回环；完整 AOT 和
真服务长时稳定性仍未签收。

2026-10-19：回环测试补服务端发出未完成 bulk 后立即关闭的路径。
Windows `xl.exe` 与 Linux `release/xl` 均确认 `command` 报错并中止旧流，
该流随后不能继续发送；原生 RESP 解析器已有逐字节未完成前缀测试。
这只覆盖断线清理，不代替真服务长时运行或完整 AOT 验收。

2026-10-18：C 边界新增一条包含嵌套数组、整数、null 和二进制 bulk 的
合法 RESP2 回复，逐字节检查所有严格前缀：完整性查询必须返回未完成且
不留错误，直接解码必须报告不完整；完整帧需还原原始字节和嵌套值。
Windows 包级 C/XLang 与 Linux ASan/UBSan 离线门禁通过；该用例不代替
真实服务长时运行或完整 AOT 验收。

同日新增自管理 Redis 的真实服务门禁入口，并在当前 WSL Redis 上验证：
C ASan/UBSan、XLang 单连接主动断线后重连、四连接各 50 轮及离线范例
同次通过；退出后确认临时实例已停止。该短时门禁不替代长时稳定性测试，
完整 AOT 的共享缺符号阻断不变。

2026-10-19：本地 TCP 回环增加空命令失败后同连接正常 PING，固定
`command` 在参数编码失败时不发送数据、不废弃调用方连接的合同。
语言边界增加非 UTF-8 的额外回复，验证 `decodeBytes` 的 `rest`
保持原始 `bytes` 而不是截断或转为字符串。Windows 离线包门禁和
Linux C ASan/UBSan、XLang 回环与双语范例门禁通过；未改运行时实现。
长时真服务及完整 AOT 仍未签收。

2026-10-21：以当前 WSL 发布运行器重新运行 `test/check_live.sh` 的
标准与 `REDIS_STRESS=1` 两种模式。临时 Redis 上的 C ASan/UBSan、
XLang 真服务、离线边界和双语对照均通过；压力模式报告四连接各
10000 轮，退出后确认端口无遗留服务。这是短时压力证据，不证明
数小时稳定性；完整 AOT 仍由共享缺符号阻断。

2026-10-21：自管理真服务门禁改用本次随机端口，不再要求 6379 空闲；
C/XLang 真服务用例与双语示例从 `REDIS_LIVE_PORT` 读取端口，单独运行
仍默认 6379。启动时核对 Redis 的 `process_id` 为本次子进程，避免
竞态下误连别人的服务。WSL `test/check_live.sh` 的 C ASan/UBSan、
XLang 真服务、四任务并发及双语输出对照通过；Windows 离线
`test/check.ps1 -SkipAot` 也通过。实现/API 未改，长时和完整 AOT 未签收。
