# redis

`redis` 提供 RESP2 编解码和基于标准库 `TcpStream` 的单命令客户端。
原生层不管理连接；C 可以直接使用长度感知的编解码接口，XL 则持有
`TcpStream`，由 `command` 组合 `send`、`wait`、`recv` 完成命令调用。
不包含连接池、自动重连、订阅模式或事务编排。

## 2026-10-30 Windows 门禁与失败现场

本轮未确认新的 RESP/客户端缺陷，也没有修改原生编解码、语言层、公开头、
manifest 或范例源。修正的是 Windows 验收入口：旧 Run 缺程序会抛异常，
不是已证实的假绿，但没有结构化启动状态；旧流程也不保留完整失败现场，
输出管道还会改变正文中的独立 CR。现复用已有进程帮助函数，记录实际
程序/参数、工作目录、原始退出码、两条输出流及耗时，不沿用 LASTEXITCODE。
普通子进程限时120秒，可选十万次复用限时600秒；不改变 Linux 既有门禁限时。
PowerShell C 运行器也在独立进程运行。输出只统一 CRLF，保留独立 CR。

新增 check_gate_diagnostics.ps1 从实际 Run 的 AST 验证11个子进程结果，
包含缺程序、静默37、预期非零、意外成功、空参数、双流、独立CR、相对
目录、超时及PowerShell脚本退出。PS5.1/7的同一自测均先在旧Run因缺少
记录失败，再于新Run通过。另从真实check.ps1入口注入静默退出37的
编译器，两种Shell均得到raw37/outer1，保留failure.txt、report.json、
commands.log、inputs.before.json；不只验证一个脱离入口的相似函数。

默认完整Windows门禁33项、44输入/工具指纹通过，含C边界/分配/线程、
TCP分片/错误/期限/500次复用和六份独立程序。新增真正的GCC C范例
构建与运行，再与C运行器、xl run、XLang JIT/AOT对照；原C运行器本来
使用TCC，不再仅凭其输出来声称GCC验证。首次新增检查因测试运行时
漏启MEMORY_DEBUG而缺少FreeAt等符号，按现有Linux夹具统一宏后通过，
没有改XRT或增加stub，失败现场保留。成功保留redis_guard_*.report.json，
只删除本次临时产物；失败保留原目录与全部记录。

Linux本轮仅验证新增C进程夹具的六种模式与源/工具指纹，未重跑完整
协议门禁、真Redis或挂载盘AOT，也未重复旧十万次压力。本地证据
build/test/redis_audit_1793335454335；最终报告
build/test/redis_guard_86061a8deb514f3ab7e9c97b2a4b2dec.report.json。
Linux夹具现场/var/tmp/xlang-redis-probe-nf2eH6tL。主线/XRT/八件冻结
发布物未变，挂载盘构建风险仍开放，Redis保持R，非整包新增发布签收。

从仓库根目录运行门禁自测：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/redis/test/check_gate_diagnostics.ps1
```

## 2026-10-28 复核

本轮修复增量扫描器将已不可能补齐的数组/大块回复继续视为不完整的问题：
读取数组头时预留所有子节点，包含尚未收到的兄弟项；字节限额同时计算帧头
与结束 CRLF。无状态检查、增量扫描与解码共用相同预算，合法完整帧范围不变。
新增 C/XLang 边界及回环测试要求立即报告畸形帧并中止旧连接，而不是等到超时。
另修复 C 范例单独释放父对象借用值的问题；新增独立调用合同审计，避免仅靠
输出对照或分配池偶然发现错误。公开 API、原生结构大小和生产分配路径不变。

固定发布物上 Windows 默认完整门禁和隔离 Redis 8.0.5 真服务门禁通过；
Linux 原生文件系统的同源码快照通过完整离线门禁，包括 C ASan/UBSan、
分配故障、线程、双语范例所有权、TCP 边界与六条独立 EXE 路径。
同一 Linux 原生快照的隔离 Redis 8.0.5 联合门禁也退出 0，包括 C/XLang
真实调用、错误后复用、显式重连、四连接各 50 轮及真服务范例 AOT；服务已关闭。
本机 Windows 挂载盘上的 Linux 核心 AOT 则两次触发既有限时 120 秒，
原生文件系统同命令约 8.7 秒。没有放宽门禁、改用其他编译器或跳过 AOT；
本包暂列 R，保留[挂载盘构建风险](../../../dev/extlib_blockers/redis_linux_drvfs_aot_20261028.md)，
不将原生文件系统通过替代挂载盘上的失败。数小时耐久仍不在本次验收范围。

### 既有验证记录

下列记录只对应各自日期和发布物，当前状态以上方复核与验收矩阵为准。

2026-10-26 完整重建 Windows/Linux 发布物后，两平台默认包门禁退出 0：
严格 C、Linux ASan/UBSan、语言边界和分配故障、离线 TCP 分片/错误/
超时/复用、核心及回环 AOT，以及公开 C/XLang 范例的独立 EXE AOT
同输出。脚本自管理的临时 Redis 真服务在两平台调用路径通过；
Windows 路径另通过四连接各 10,000 轮真实服务压力和同连接
100,000 次离线请求。测试结束后无 Redis 进程残留。本包按当前
Windows/Linux x64 短时 RC 范围签收；数小时耐久、集群、自动重连及
未列出的 RESP 模式不在合同内，也不等于整个 xlang 产品发布准入。

2026-10-25 复验：Windows 默认 `test/check.ps1` 全通过，包括严格 C、
分配故障、线程、XLang 网络边界、三条 TCP 路径及双语范例的独立 EXE
构建和运行。`test/check_live.sh --windows-xl` 在 WSL 自管理 Redis 上
也通过 C/XLang 固定输出、二进制键值、错误后复用、断线重连与四连接
各 50 轮；同一 XLang 真实服务范例构建为独立 EXE 后，输出与 JIT
逐字一致。此模式现从 `release` 工作目录运行 `xl.exe`，避免测试脚本
触发已记录的根目录 VFS 头文件路径故障。同日使用当前 Linux 发布版
通过自管理 Redis 真服务门禁：严格 C ASan/UBSan、XLang JIT，核心、
预检、回环、连接复用及离线/真服务范例的独立 EXE AOT 均通过；两份
范例分别和 C 输出逐字一致。旧 Linux 发布物的 `cstring` 类型错误
已解除；短时测试不等于长时耐久签收。

2026-10-24 当前 Windows 发布版复验：默认 `test/check.ps1` 完整通过，
包括严格 C、边界/分配故障/线程、XLang 边界/分配故障、现行 TCP API 的
预检、分包、畸形回复、断线、限额和期限，以及核心、回环、公开范例的
独立 EXE 构建与运行。C/XLang 双语范例逐字输出一致；另单跑同连接
100000 次 JIT 请求通过。网络回环使用 `NetEngine`、`TcpListener`、
`bytes`、`NetStatus` 和 `TcpWait` 的当前合同。此处是 Windows 离线包级
签收，不代表真实 Redis 长时运行或当前 Linux 发布物的完整复验；
下文旧链接阻断记录对应当时的发布物。

同日用当前 Windows `xl.exe` 和 WSL 内自管理的 Redis 复验真实服务：
`test/check_live.sh --windows-xl` 的 C/XLang 固定输出、二进制键值、
错误回复后复用、服务端断线后重连及四连接各 50 轮均通过；
`REDIS_STRESS=1` 再通过四连接各 10,000 轮。三份真服务 XLang
范例/测试已改用当前 `NetEngine`、`NetAddress`、`TcpStream` 接口。
这属于短时实测，不代替数小时运行或当前 Linux 发布版验收。

## 使用与边界

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
可使用 `decodeBytes(bytes)` / `decodeBytes(Buffer)` 和对应的
`isCompleteBytes` 重载解析原始字节，避免文本路径的编码限制。
`release/examples/extlibs/redis.xl` 与 `redis_c.c` 的成对范例先检查一条
未收齐的二进制 bulk 回复，再解析含 `0xff` 的完整回复并逐字节核对，
无需连接真实 Redis 服务。
`isComplete` 对不完整回复返回
`false`，对畸形回复抛 `ValueError`。编码和解析均按**字节长度**处理，
字符串中的 `\0` 不会截断数据。命令最多十万个参数；回复输入最多 64 MiB，
帧头、内容与结束 CRLF 都计入字节限额。整条回复累计最多十万个值节点，
包含根值及空数组，因此根数组最多 99999 项，嵌套后代也占用同一额度；
嵌套深度最多 64 层，根值为第 1 层。这些预算在头部已能确定非法时立即拒绝：
例如 `*99999\r\n` 是合法未收齐前缀，`*100000\r\n` 已是畸形回复；
64 层非空数组头不可能再补一个合法子值。根 bulk 的最大正文为 67108851 字节，
不是完整的 64 MiB。输入已占满字节限额而首帧仍未结束时，也属于畸形回复。
含非 UTF-8 字节的 RESP error 负载在原始解码结果中仍为 `bytes`，
不会在解码层丢失字节；`command` 才将完整错误回复转换为包级异常。

`command(stream, args, timeoutMs=5000, maxReplyBytes=67108864)` 在已有
`TcpStream` 上发送**一条**命令并通过返回 `bytes` 的 `recv` 读取**一条**回复。
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
`xrtValueObjectGet`、`xrtValueArrayAt` 取得的子值只是借用；没有另行 retain
时不能单独 release，由拥有它们的父值统一释放。C 范例将这类值与拥有值分开命名。

包级验证：

- 在 `release` 目录运行 `xl run examples/extlibs/redis.xl` 或
  `xl run examples/extlibs/redis_c.c`，输出一致，均不依赖外部服务。
  C 范例直接包含原生实现；从仓库根目录调用时也可使用
  `tools/run_extlib_c.ps1 -Package redis -Source release/examples/extlibs/redis_c.c`。
- `xl run extlibs/redis/test/boundaries.xl`
- `tools/smoke_extlib_catalog.ps1 -Package redis` 会发现包清单登记的无设备
  边界测试和中央扩展库用例；两者均按当前非空命令合同验证。回环、真服务与
  压力测试由聚焦门禁单独运行。
- `xl run extlibs/redis/test/command_loopback.xl` 在本机随机端口测试
  `TcpStream` 命令往返、分段回复、错误回复，以及持续滴送不完整回复时的
  整次调用超时；还检查自定义短超时和回复字节限额。
  畸形回复、超限和超时后验证旧流不再处于 `open` 状态。
- `xl run extlibs/redis/test/preflight_repro.xl` 单独检查非法期限、限额、
  空命令均在发送前失败，且连接仍保持 `open`。分配故障由独立的
  `test/allocations.xl` 和 C 故障门禁验证，不在活跃网络线程上启用
  全局分配失败注入。
- `test/check.ps1` 是不依赖真实服务的 Windows 包级门禁；普通模式包含
  C 边界/分配故障/线程、XL 边界与分配故障、TCP 回环和双语范例对照，
  并构建运行核心及 C/XLang 范例 AOT，将后两者输出与 GCC C 对照。
  `-SkipAot` 只运行离线 C/JIT 快测，不表示发布签收。`-Stress` 另跑
  `test/reuse_loopback.xl` 的十万次同连接请求（每 17 次有一个 Redis
  error 回复）；2026-10-24 本机 500、10,000、100,000 次均通过。
  这是协议与连接复用探针，不替代真实 Redis 的长时验收。
- 包内 C/XLang 与中央 XLang 用例均固定空命令返回范围错误，随后 `PING` 编码恢复正常；
  不增加空命令兼容写法。
- 用 `tools/run_extlib_c.ps1` 运行 `test/test_redis.c`、
  `test/test_redis_alloc.c` 和 `test/test_redis_threads.c`；故障测试逐点拦截
  文本/二进制命令编码及嵌套回复解码的分配，要求失败保留内存错误类型，
  清除故障后生成的文本 RESP 与固定字节向量一致。
  `bash release/extlibs/redis/test/check.sh` 在独立 Linux 进程中运行这三组
  C 测试并启用 ASan/UBSan 泄漏检查，还运行中央及包内 XLang 边界、500 次本地
  TCP 回环复用及 C/XLang 范例逐字节对照；默认还验证核心、回环及
  C/XLang 公开范例独立 EXE，且无需 Redis 服务。
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
- WSL/Linux 已安装 Redis 并监听 `127.0.0.1:6379` 时，可从仓库根目录
  运行 `REDIS_LIVE=1 bash release/extlibs/redis/test/check.sh`。该可选分支
  以 ASan/UBSan 跑 C 真服务用例，再跑 XLang 单连接与四连接并发用例；
  默认不连接服务。2026-10-15 在 Redis 8.0.5 上三组均通过。
- WSL/Linux 已安装 `redis-server` 和 `redis-cli` 时，可运行
  `bash release/extlibs/redis/test/check_live.sh`：它选择本次临时端口，
  只启动绑定 `127.0.0.1`、关闭持久化的实例，并以 Redis 报告的进程 ID
  核对服务归属。已有 Redis 不会被连接或关闭；成功或失败均清理本次服务。
  真服务 C 示例和 C 边界先于 XLang 检查运行，故语言编译失败时仍可明确
  区分原生接口是否已经通过；整个脚本仍返回失败，不作降级验收。
  在 WSL 内也可加 `--windows-xl`，使用同一自管理 Redis 实例验证当前
  Windows `release/xl.exe` 的真实服务调用与 C/XLang 固定输出；脚本经
  `WSLENV` 仅传递本次端口。`REDIS_STRESS=1` 额外运行四连接各 10,000 轮。
  此模式需要 WSL Windows 互操作与已构建的 Windows 发布版，不表示
  Linux `release/xl` 已通过。
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

`test/test_redis.c` 还实际构造十万节点边界及完整 64 MiB 回复，不缩小生产
上限替代测试；所有头部严格前缀均交叉比较两种检查器，并以独立预期区分
合法未完成和畸形输入。`test/test_example_lifetime.c` 单独审计 C 范例直接
释放借用值的调用，两平台默认门禁均运行；这是此范例的合同回归，不是通用
引用计数分析器。Linux 门禁成功才删除工作目录，失败保留原始退出码、
命令/行号、管道状态、编译器版本、输入指纹和产物；可用 `TMPDIR` 指定
持久的测试目录，避免 WSL 重建会话时丢失 `/tmp` 现场。运行前后同时核对
包输入、XRT 头、CLI 与 `libxlc.so` 的指纹。

## 历史阻断记录

以下只记录当时的发布物，不描述当前故障或恢复旧接口。

2026-10-24 及以前，XL AOT 曾因主线缺少 `xl6ValueDataSnapshotV1` 而失败，见
`dev/extlib_blockers/text_snapshot_link.md`。新增无输出
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
当时真实 Redis 单连接成功/错误/主动断线后显式重连及四连接短轮次并发路径已验证，
C 原生宿主也经过真服务门禁；当时尚未覆盖长时运行和完整 AOT，未签收发布验收。
当前 API 仍不承诺自动重连或自动重试。

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
