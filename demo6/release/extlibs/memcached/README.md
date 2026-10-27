# memcached

Memcached 文本协议编解码与基于标准库 `TcpStream` 的单命令客户端。连接由
调用方创建和持有；扩展库不实现连接池、自动重连、SASL、二进制协议或
服务发现。C 和 XLang 使用相同的长度感知帧规则。

## XLang

```xlang
import memcached

auto engine = NetEngine(NetEngineOptions().withWorkers(1).withBackend(NetBackend.select))
engine.start()
TcpStream stream = TcpStream(engine, NetAddress("127.0.0.1", 11211))
assert stream.wait(TcpWait.open, 5000) == NetStatus.ok
assert memcached.set(stream, "key", "value") == "stored"
var value = memcached.get(stream, "key")
assert value == "value"
stream.close()
engine.close()
assert engine.wait(5000)
```

`set` 和 `add` 接受 `string` 或 `bytes`；有效 UTF-8 的返回值为
`string`，非 UTF-8 返回 `bytes`。`get` 未命中返回 `null`，命中空值
返回空字符串，这两种状态不同。`getMany` 返回命中项数组，未命中的键
不在数组内；`del` 和 `touch` 返回是否生效。`stats` 返回状态字典，
`version` 返回服务版本，`flushAll` 清空服务的全部键，需谨慎调用。
`set/add` 的到期秒数默认为 0。便捷操作的超时默认为 5000 毫秒，范围
为 1..2147483647 毫秒。`get/getMany` 可把 `maxReplyBytes` 从默认
64 MiB 收紧；单条回复的硬上限是 64 MiB。收到不完整、畸形、超限或额外回复时，中止已
失同步的连接；调用方需要重新连接。完整错误回复抛 `ValueError`，
连接仍可复用。便捷操作不支持流水线并发访问同一 `TcpStream`。

`buildGet`、`buildSet`、`buildAdd`、`buildDelete`、`buildTouch`、
`buildStats`、`buildFlushAll`、`buildVersion` 提供文本协议命令。
`frameSize(Buffer)` 返回首条完整响应的字节数，0 表示还需字节，-1
表示畸形；`parseResponse(Buffer)` 要求恰好一条完整响应。
`VALUE` 返回 `{status: "value", items: [...]}`，其中每项包含
`key`、`flags`、`bytes`、`data`，带 CAS 的回复另含无符号十进制字符串
`cas`；`STAT`
返回 `stats` 字典。所有 key 长度为 1..250 字节，不能包含空白和控制
字符，命令行上限为 1 MiB。单帧至多 100000 个 VALUE/STAT 项。

## C

包含 `include/memcached_native.h`，将 `memcached_request.c` 和
`memcached_response.c` 编入程序。构造函数返回的 `char*` 由调用方使用
`xrtFree` 释放；`xmc_parse_response_span` 返回的 `xvalue*` 由调用方
使用 `xrtValueRelease` 释放。输入 `xstrview` / `xbytesview` 只借用。
`xmc_frame_size` 允许增量扫描；单次解析应传入完整且无尾随数据的
字节视图。`xmc_build_get`、`xmc_build_set`、`xmc_build_add`、
`xmc_build_delete`、`xmc_build_touch` 返回拥有型 `char*`；
`xmc_build_stats`、`xmc_build_flush_all`、`xmc_build_version` 返回
借用的静态字符串，不得释放。`xmc_build_get` 接受普通 XRT `xvalue`
数组，XLang 的静态
泛型数组由语言层 `buildGet` 构造，不混用两种数组布局。

## 验证

- Windows：`powershell -File release/extlibs/memcached/test/check.ps1`
- Linux：`bash release/extlibs/memcached/test/check.sh`

日常门禁包含 C 协议边界、故障分配、Linux ASan/UBSan、XLang JIT/AOT、
本地 TCP 分包/二进制数据/错误后连接复用/半包、超时与限额中止，以及 C/XLang
同结果范例。离线范例是 `examples/basic_c.c` 和 `examples/basic.xl`。

真实服务门禁需要安装 `memcached` 和 `ss`，并自动启动临时实例：

- Linux：`bash release/extlibs/memcached/test/check_live.sh`
- WSL 测 Windows 发布版：`bash release/extlibs/memcached/test/check_live.sh --windows-xl`
- 短时并发压力：在上述命令前设置 `MEMCACHED_STRESS=1`，执行四连接各
  1000 轮 set/get/delete/未命中。

`examples/live.xl` 与 `examples/live_c.c` 可单独连接现有服务，默认
端口 11211，或使用 `MEMCACHED_LIVE_PORT` 指定端口。两者均校验普通值、
非 UTF-8 二进制值、空值与未命中、多键读取、add/touch、stats/version、
重复删除和 flushAll，输出逐字节相同。当前已在
Windows/Linux 发布版的 JIT 和独立 EXE AOT 下通过临时真实服务测试及
上述并发短压；数小时耐久、真实生产流量和服务集群未验收，不把短测
解释为完整生产签收。
