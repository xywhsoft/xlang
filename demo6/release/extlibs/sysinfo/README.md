# sysinfo

`sysinfo` 提供 Windows/Linux 主机运行时信息：`osName`、`osVersion`、`arch`、
`hostname`、`cpuName`、`cpuCount`、`memTotal`、`memAvailable`、
`memUsagePct`、`pageSize` 和 `uptime`。

```xlang
import sysinfo

println(sysinfo.osName(), sysinfo.arch())
println(sysinfo.memAvailable())
```

字符串始终为 UTF-8 的独立结果，不共用全局缓冲区。C API 见
`sysinfo_native.h`，字符串结果必须用 `xrtFree` 释放；XLang 包装会自动释放。
每个公开查询开始时清除上一条 XRT 错误；探测不可用仍按下述规则返回默认值，
但分配失败会返回空结果并保留内存错误，不会伪装为 `unknown`。
原生实现使用 XRT 基础分配/释放、UTF-8 校验和严格整数解析，包清单声明
`XRT_MODULE_UNICODE` 与 `XRT_MODULE_NUMBER_INTEGER`；无输出边界 AOT 保持裁剪构建。
本次 Windows 构建的该 AOT 产物为 609,280 字节。
Windows 版本号读取实际系统的 major/minor/build，不返回硬编码版本号；
主机名和 CPU 名通过 Unicode 系统接口读取后转 UTF-8。注册表 CPU 名即使没有
终止符也保留完整文本，异常的奇数字节宽度被拒绝。Linux 的 OS/架构
来自 `uname`，CPU 名和 uptime 从 `/proc` 读取；系统返回非 UTF-8 字节时，
文本返回 `unknown`，不会把非法编码交给 XLang string。不可用时文本返回 `unknown`、
数值返回 0。内存值是瞬时快照，相邻调用间可能变化。
Linux CPU 名称读取完整字段，不会截断长型号，也不会把长行的后续片段误认成新字段。
若型号字段只有空白，返回 `unknown`；Linux 内核的 CPU 信息字段随架构而异，
例如原生 ARM64 不一定提供 `model name`，这种情况下同样返回 `unknown`，
不把架构名冒充具体 CPU 型号。
Linux 的 `memAvailable` 读取 `/proc/meminfo` 的 `MemAvailable`，计入内核估算可回收的缓存，
不是只统计当前空闲页；`memUsagePct` 使用同一可用内存口径。读取失败时，
公开的 `memAvailable` 和 `memUsagePct` 都返回 0，不会把探测失败误报为 100% 使用率。
`MemAvailable` 必须是非负十进制整数并带 `kB` 单位；负数、数字尾部垃圾或
整数溢出按探测失败处理，不把异常文本解释为巨大内存值。
`cpuCount` 返回全系统在线逻辑处理器数；Windows 使用跨所有处理器组的
`GetActiveProcessorCount`，不使用仅统计当前组的 `SYSTEM_INFO.dwNumberOfProcessors`
（参见 [Microsoft 的字段说明](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/ns-sysinfoapi-system_info)）。

在 `demo6` 目录运行定向验证：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/sysinfo/test/check.ps1 -SkipAot
bash release/extlibs/sysinfo/test/check.sh
```

去掉 `-SkipAot` 还会构建并运行独立可执行文件。C/XLang 范例输出在同一台
机器上比较，不把机器名等动态值写死为黄金文件。
C 范例自包含原生实现，可从 `demo6` 目录直接运行：

```powershell
release/xl.exe run release/extlibs/sysinfo/examples/sysinfo_c.c
```

Windows/Linux 包门禁将这一路径与 GCC 编译运行、XLang 范例的输出对照。
当前 Windows 严格 GCC、C/XLang 双语对照及边界 AOT 通过；Linux 包级门禁
同次运行严格 GCC、C ASan/UBSan、XLang 边界及双语范例逐字节对照。
带文本输出的范例 AOT 仍缺共享运行时符号
`xl6ValueDataSnapshotV1`，见 `dev/extlib_blockers/text_snapshot_link.md`。
六个整数 C 结果统一使用 `int64_t`，与 XLang `int` 在 Linux LP64 上保持一致。
本机未具备多处理器组，相关计数路径只按 Windows API 合同与本机调用对照验收。
Linux C 测试以前后两次 `/proc/meminfo` 快照对照 `memAvailable`，容许短时内存波动；
另以链接器夹具模拟该文件不可读，核对使用率错误边界。Windows 正常内存查询路径未改。
Linux `uptime` 在 `/proc/uptime` 返回负数、非有限数或超出有符号 64 位秒数
范围时返回 0；包级 C 夹具覆盖正常小数秒和 `2^63` 边界，避免浮点到整数越界转换。
Linux 另用链接器夹具注入非 UTF-8 的 `gethostname`/`uname`/`cpuinfo` 返回值，核对文本降级；
还注入超长 CPU 型号、跨缓冲的伪字段和相似字段名，核对完整读取及字段边界。
这些用例进入包内 ASan/UBSan 门禁。
Windows 另用包内伪注册表夹具验证有/无终止符及异常字节宽度，不访问或修改真实注册表。
该夹具还注入 UTF-8 转换的首次分配失败，验证错误传播和恢复后再次调用。
Windows 包级脚本为每次运行创建独占构建目录，退出时仅清理该目录；并行执行不再共用缓存对象或测试程序。

2026-10-17：Linux 链接器夹具先复现负数 `MemAvailable` 被 `%llu` 读成巨大
无符号值；现用 XRT 严格整数解析并检查单位，补负数、尾部垃圾和溢出负例。
Linux C ASan/UBSan、XLang 边界与双语对照，Windows 严格 C/XLang 门禁及
无输出边界 AOT 通过；该 AOT 仍为 609,280 字节。带输出范例 AOT 的共享
缺符号阻断不变。

2026-10-16：Linux 包级脚本加入 C 范例与 XLang 范例同输出门禁，配合包内
XLang 边界、非 UTF-8 系统文本及不可读 `/proc/meminfo` 的 C sanitizer
用例同次通过。Windows 完整脚本复跑至边界 AOT 通过，带输出范例构建仍缺
`xl6ValueDataSnapshotV1`；本包未改主线，尚未签收完整 AOT。

2026-10-19：Windows 包级构建从固定 `build/test/sysinfo_guard` 改为独占目录，避免并行门禁覆盖 XRT 对象与可执行文件；退出后核对路径并清理本次产物。`-SkipAot` 下 ABI、严格 C、注册表夹具、XLang 边界和三条范例路径全部通过，未留下独占目录。完整 AOT 的共享缺符号阻断未变。

2026-10-21：Windows 注册表与 Linux `/proc/cpuinfo` 的型号字段只有空白时，
原返回空字符串；双平台 C 夹具先红后绿，现按文本不可用合同返回
`unknown`。Windows 严格 C/XLang 聚焦门禁、Linux C ASan/UBSan 与
XLang 双语门禁通过。有效型号与分配失败路径不变；完整 AOT 仍待主线修复。
