> 历史工作树记录：以下结果对应文内冻结版本，不自动代表主目录当前 0.6.0；需在当前发布物上复验。

# Redis Linux 挂载盘 AOT 限时风险（2026-10-28）

## 状态与范围

R：保留本机 `/mnt/c` 上的构建耗时风险，不是已确认的 RESP 协议或内存错误。
Windows 完整离线及真 Redis 门禁通过；Linux 原生文件系统上的同源码、同发布物
完整离线及真 Redis 联合门禁也通过。不能用后者覆盖原挂载工作树中默认门禁退出 124 的事实。
本任务不修改 xlang 主线，也不放宽限时、裁掉运行时依赖或换成其他编译器验收。

编译器源码基线为 `d64672903`，不是最新 master。Linux `release/xl` SHA-256：
`E68B095FFD44D0E21B48F54E0C612EA352A731B97E1F17810B2E2C628C0CD760`；
`libxlc.so`：`2ABFE80902017A702489371DCE9D77B3DAC20844746C9C3496F476C47122262B`。
现场为 Windows 工作树、WSL2 Ubuntu 26.04.1 x64；Linux 验证器为 GCC 15。

## 复现

从 Windows 挂载工作树的 `demo6` 目录运行：

```sh
bash release/extlibs/redis/test/check.sh
```

C ASan/UBSan、分配故障、线程、范例借用值合同、语言边界、TCP 分包/错误/期限及
500 次连接复用均已通过。随后默认脚本的这一步未在 120 秒内结束：

```sh
timeout 120s ./release/xl build release/extlibs/redis/test/aot_core.xl -o /tmp/redis_core
```

实际目标使用门禁独占目录；这里的名字只用于说明。门禁捕获到：

```text
exit=124
line=94
command=timeout 120s ./release/xl build release/extlibs/redis/test/aot_core.xl -o "$work/redis_core"
pipeline=124
```

不运行其他 Redis 用例，单独以相同 120 秒限时重复该构建，仍退出 124：

| 指标 | Windows 挂载盘 | Linux 原生文件系统快照 |
| --- | ---: | ---: |
| 退出码 | 124 | 0 |
| wall time | 120.05 秒（被限时终止） | 8.71 秒 |
| user / system | 10.39 / 10.77 秒 | 7.97 / 0.68 秒 |
| CPU 使用比例 | 17% | 99% |
| 最大 RSS | 53804 KiB | 56160 KiB |
| 自愿上下文切换 | 630519 | 91 |

挂载盘进程采样为 `D`，`wchan=p9_client_rpc`，正在 `openat`；文件描述符涉及
`src/runtime/xl6_array_reference_admission.c` 及其头文件。中途采样已读取约 306 MB，
发起 42901 次 read 类调用。WSL 可用内存约 14 GiB，swap 未使用，没有发现内存压力。
这些证据指向大量源/头文件访问的挂载盘开销，不是死循环或 OOM 的证据。
两次测量不保证系统负载和缓存一致，不应当作通用性能倍数或最终根因结论。

## 对照条件

在 `/var/tmp` 创建一次性 Linux 原生文件系统快照，复制 `src`、`res`、`lib/xrt`、
`lib/xruntime`、Linux 发布物与声明头、Redis 包、四份双语范例和中央用例。
源码树、XRT 目录及 Redis 包以 `diff -qr` 核对一致；包、头、范例和实际发布物
由门禁运行前后 SHA-256 核对。没有改脚本、源码或其任何限时，再执行同一默认门禁。
它退出 0，包括核心、三份 TCP 用例和公开 C/XLang 范例的六条 AOT 路径。
额外重复一次原生文件系统核心构建得到上表的 8.71 秒，再执行该产物退出 0。
随后原生快照的 `check_live.sh` 也完整退出 0，包括 C sanitizer、Redis 8.0.5
真实调用、四连接各 50 轮、二进制键值与重连，以及真服务范例 AOT；测试服务已关闭。

这不是新增发布目录或永久维护的副本，只是固定输入的验证现场。最初 `/tmp`
现场在脚本失败时曾保留，随后观察到 WSL 的 `/tmp` 被重新创建，原目录不再存在；
没有确认该外部状态变化的原因。上述原始诊断已由实际命令输出转录，后续诊断
改用支持的 `TMPDIR=/var/tmp`，未修改 WSL 配置或终止其他线程的任务。

## 处理与关闭条件

- 包内 RESP 和范例所有权修复保留，不能回退成只等待更多数据来掩盖错误。
- 主线可先量测 artifact runtime plan 的源码数、重复头文件展开和文件调用，
  检查按实际依赖裁剪及减少重复读取的空间；相关循环在
  `src/api/xl_api.c` 的 `xlAddPrivateLanguageRuntimeSources`。本包不改该边界。
- 后续在固定平台合同下排查挂载盘构建，保留冷/热、负载与实际 I/O 指标。
  关闭风险需有根因与修复或明确的平台合同，不能只延长超时、跳过 AOT 或多跑至绿。
- 当前只增加这一具体构建风险，不把它反推为 HCL/SQLite 历史无诊断失败的共同根因。

Linux 门禁已验证失败时保留原始命令、状态及输入指纹：以 `/bin/false` 作为
`CC` 的受控探针退出 1，`failure.txt` 准确记录 `"$cc" --version`，没有被记为成功。
