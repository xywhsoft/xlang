> 历史工作树记录：以下结果对应文内冻结版本，不自动代表主目录当前 0.6.0；需在当前发布物上复验。

# INI 门禁的 GCC 内部错误与 WSL 中断记录

状态：R（工具链/环境风险，原因未闭环），不是已确认的 XLang 主线 BUG。
普通扩展库任务不修改 GCC、XLang 或 XRT 快照，不换优化级别来掩盖失败。

## Windows 原始失败

2026-10-28 的第二阶段输出限额修复完成后，默认 `ini/test/check.ps1`
通过严格语法检查，随后在编译共同 XRT 测试对象时返回 1：

```text
during GIMPLE pass: evrp
release/extlibs/ini/test/xrt_runtime.c: In function '__xrtRootWindowsPart':
release/extlibs/ini/test/xrt_runtime.c:4: internal compiler error: Illegal instruction
```

这一步尚未编译 INI 的改动；`test/xrt_runtime.c` 只有完整 XRT 宏和包含。
同日第一阶段默认门禁已经用同一版本编译成功，但这不能抹去后来的 ICE。
失败目录为 `build/test/ini_guard_c13efe2db8d64c0cbbe6ebc34097275f`，
保留了 `commands.log` 和 `inputs.before.json`。相对 `demo6` 的复现命令：

```text
gcc -std=c11 -O2 -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/ini -c release/extlibs/ini/test/xrt_runtime.c -o build/test/ini_gcc_repro.o
```

实际编译器是 PATH 优先的 `E:/software/w64devkit/bin/gcc.exe`，GCC 16.2.0。
本次核对的 SHA-256：

| 输入/工具 | SHA-256 |
| --- | --- |
| lib/xrt/xrt.h | 049C180A43597D5476F5357CC9A7D1603B439765A3E3550437F7D49B5B3E55F4 |
| ini/test/xrt_runtime.c | 7F1C68230507AC60F8229FC77BA442D116AE6924B9952BF129E0C0600D9DBC5A |
| gcc.exe | 9A52DE3E3AF8F4896E963C4B68ED159F049954775DECB20E421CD75D51AAB6ED |
| cc1.exe | B5536BE9710FB9EECE19DE8CC9469D5ECD96D8282418E61D529A1394A8921012 |

核对失败时保存的全部输入指纹后，单独重复相同编译参数，输出
`xrt_retry.o`，退出 0；对象 SHA-256 为
`3CA619AE89B16F0EFC568D6788E423027AB0B58B855BCD0C667C547D380F4B7C`。
这确认失败不是持续必现，但没有证明原因，更不是修复。

没有为了签收使用 `-O0`、关闭 evrp、换编译器、替换 XRT 或借用旧对象。
一次观察到的系统可用内存充足也不能排除先前的环境或编译器问题；
本任务不据此推断硬件、并行线程或其他项目有责任。

随后从正常入口重新执行完整默认 Windows 包门禁，退出 0：严格 GCC、
三个 C 边界、独立 Python 互通、TCC、全部语言用例及新旧双语范例的
独立 EXE 均通过，运行前后输入指纹一致。仍不能用这个结果关闭一次
未定位的 ICE；失败日志及单独复编对象保留，没有覆盖原始诊断。

## Linux 中断与快照失效

同一时段的 Linux 默认门禁到达 C sanitizer 阶段后，外层 WSL 调用退出 1，
没有正常的 ERR/EXIT 收尾。之后检查到：

- UTC 07:19:01 时 `/proc/uptime` 只有 98.88 秒，init 约启动 97 秒；
  boot id 为 `72f90904-bb96-4ff2-8e12-2be4606089f3`。
- 运行目录创建于本地时间 15:15:33，早于上述本次启动；内核记录 EXT4
  recovery complete，以及 journal corrupted or uncleanly shut down。
- `/var/tmp/xlang-ini-mWfJFg` 的输入指纹、compiler.txt、output.log 变成
  零字节；刚复制的 `xlang-ini-src-bq8hmq` 中 XRT 和包文件亦有零字节。

因此该次运行属于非正常重启后的无效现场，不能当作解析器、sanitizer 或
语言测试失败，也不能拿失效的输入继续测。没有确定谁或什么触发了重启，
没有在本任务中停止 WSL、重启系统或修改虚拟机配置。
重新从 Windows 工作树建立独占 `/var/tmp` 快照，目录差分确认一致，并在
验证前将该诊断快照同步到文件系统。先前第一阶段已完整通过的快照仍可读；
不能把这次中断与 HCL/SQLite 的旧风险认定为相同根因。

新快照为 `/var/tmp/xlang-ini-src-sXUNhN`，在上述 boot id 下从正常入口
执行默认 `check.sh`，退出 0，包含 C ASan/UBSan、三个 C 边界、Python
互通、XLang JIT/AOT 和所有双语对照。快照复制前后的源码目录已差分，
门禁核对输入与实际运行库的 SHA-256；没有改限时或脚本来跳过阶段。
此正常结果与那次失效运行分开保存，也不关闭已有挂载盘 AOT 耗时风险。

## 关闭条件

### 2026-10-30 门禁诊断补强，不关闭 ICE

本轮未得到可归因的 ICE 复现。Windows 门禁统一使用既有有界进程工具，
默认单项 120 秒，分别记录 stdout/stderr、工作目录、真实状态和退出码；
gcc、cc1、Python、PowerShell 及工具源码加入输入指纹。编译参数仍为原
配置，未关闭 evrp、换优化级别、自动重试或复用旧对象。

- PowerShell 5.1/7 的 AST 自测均确认：旧 Run 会抛缺程序异常，但跳过
  进程结果记录；新 Run 通过十个子进程结果检查。此问题不是已证实的
  假绿，更不是这次 ICE 的根因。自测含静默退出 37、预期拒绝、意外
  成功、空参数、双输出流、独立 CR、相对目录与一秒超时。
- 两种宿主的默认完整门禁各通过 73 个子进程阶段，含 14 份独立程序。
  两个私有访问负例保留真实 fail/exit=1，其他 71 项正常成功；不是将
  非零状态改成 0。各 51 个实际输入/工具前后不变，GCC/cc1 指纹仍与
  原始失败记录相同；本次无编译警告或 ICE，但不据此关闭风险。
- 另用现有 c_run_exit_code.c 构建静默退出 37 的隔离编译器替身，
  只在子进程 PATH 中置前，从两种宿主的真实 check.ps1 入口验证：
  外层脚本失败为 1，GCC version 的内部原始状态保留 fail/37/空输出。
  两次均保留原目录、命令、failure.txt、输入与最终结构化报告。
  正式门禁不用该替身；父进程 PATH 和真实 GCC 未变。
- Linux 仅对新增 C 诊断夹具严格编译和运行，覆盖退出 37、双流、独立
  CR、相对目录及超时 124；不是本轮 Linux INI 完整回归。原生现场
  /var/tmp/xlang-ini-gate-probe-TUxloA 保留，夹具源复制与前后哈希一致。

Windows 完整报告：build/test/ini_guard_5e93e3ce64f24836a83eccbba0dc570c.report.json
和 build/test/ini_guard_06de76f4f65d4ef188fbf20fc44fb6a4.report.json。
前者为 PowerShell 5.1，后者为 PowerShell 7；两次代码相同，后者还包含
新追加的官网验证说明，其包内 34 个输入前后不变。两次成功均清理了
原临时产物，报告在目录旁保留。

红绿、真实入口故障及前后指纹证据在
build/test/ini_gate_before_70d7234e78cc4680af2b1edd069f0548。
自测最初曾因 PowerShell 5.1 的 JSON 数组管线展开差异误判记录数，
修正测试读取后两种宿主均通过，不把该测试假设当成 INI 产品 BUG。

以下关闭要求继续有效，状态仍为 R：

2026-10-29：包内另确认并修复拥有型流关闭覆盖 XRT 首因，以及 Reader
关闭OOM误报IO；14组C复合失败和两平台默认完整门禁通过，Windows
本次相同GCC参数的运行库编译没有复发ICE。该修复与evrp内部错误
无因果证明，不修改上述历史结论或R状态；主线、XRT、编译器未改。

- 固定实际 GCC/cc1、预处理输入、选项和平台，保留再次失败的原始状态；
  必要时用 `-freport-bug` 在独占目录收集诊断，不自动上传源码。
- 查明原因并修复，或形成有依据的受支持工具链/环境约定后，再运行 INI
  默认完整门禁。一次重试为绿、提高限时或换环境都不能单独关闭此风险。
- 单独记录 Linux 正常启动下的新快照验证；不使用已零字节化的现场，
  不把这次可识别的中断扩散为普通 100 包全体回归失败。
