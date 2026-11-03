# 独立 TUI 更新器

一套 C/XRT 代码，各平台分别编译。XRT 单头以 `xrt_profile.h` 裁剪，独立编入可执行文件；不加载 XGE、引擎、外部 TLS 库或客户端中的 XRT DLL。Windows 终端输出由 XRT 转为 Unicode，光标/输入适配使用传统控制台 API，不强制开启 VT；Linux 使用终端序列和 termios。不支持完整终端时输出普通文本，显示失败不阻断更新。

正常输出位于 `release/<platform>/update[.exe]`。默认 URL 为 `https://xcompiler.com/download/update_<platform>`，默认根目录是自身所在目录。仅靠该可执行文件即可开始引导；官方线上目录和签名正确部署后才可下载正式更新。

```powershell
release/win_x64/update.exe
release/win_x64/update.exe --check --no-launch --plain
release/win_x64/update.exe --offline E:/xlang-updates/win_x64 --no-launch
release/win_x64/update.exe --root D:/Apps/xlang --recover-only --no-launch
```

`--check` 只检查目标文件，不修复未完成安装；`--check-version` 是 IDE 使用的轻量检查，不扫描全部安装文件。存在未完成事务时，先关闭 IDE 并运行正常更新或 `--recover-only`。同版本正常运行仍会修复缺失或损坏文件。退出码 0 为成功/无需更新，2 表示普通更新，4 表示必须更新，1 为失败，3 为提交前取消。非交互模式加 `--plain`，不会等待按键。

下载显示标题、平台/版本、更新简介、当前文件、当前文件和总下载进度。下载、校验时可取消；提交时完成事务或恢复旧文件。失败时交互终端提供 R 重试、Esc/Enter 退出；完整错误记录在安装目录 `.xlang-update/update.log`，超过 1 MiB 时轮换一份。安装目录需要写入权限；权限不足时保持现有文件并报告错误。

`version.txt` 支持 `{"version":3,"forceUpdate":true}`。强更标记须与签名清单一致；普通数字版本仍可读取，按非强更处理。`--cached-policy` 仅验证本机已缓存的签名强更策略，无网络访问；`--check-version` 在网络错误时仍保留有效强更要求。IDE 不会仅凭未签名的网络标记限制用户使用。

## 安装与恢复

1. 获取版本号、签名清单、再次获取版本号，三者一致才采用清单。
2. 验证固定公钥、平台、路径、文件/分片大小、哈希及受管文件范围。
3. 下载到 `.xlang-update/cache`，保留已完整验证的对象供失败后重用。下载、LZMA 解压和哈希按块处理；限制解压字典和输入规模。
4. 所有文件准备在同卷 `txn/stage`，保存并刷新事务日志后才替换运行文件。
5. 旧文件逐一改名到 `txn/backup`，新文件改名到目标位置。逐项校验旧身份，拒绝覆盖安装期间被其他操作修改的文件。
6. 提交完成后保存收据，清理事务和下载缓存。失败按日志恢复；备份缺失或文件被外部修改时保留日志并停止，不假报恢复成功。

Windows 在安装目录内启动时，复制自身到系统临时目录并释放原进程，避免自身 EXE 被占用。临时进程等待原进程与 IDE 确实退出后才修改安装；退出后由同一程序的受限清理模式删除已结束的临时副本，不使用外部命令或递归删除系统临时目录。独立运行不会强行终止其他程序，文件占用会导致恢复/重试提示。在线清单不能删除更新器自身。

Windows 默认引导进程的退出码表示交接是否成功；需要获取最终安装退出码的自动化，应从安装目录外运行更新器并指定 `--root`。检查模式不需要自迁移，直接返回检查结果。`--staged`、`--bootstrap-pid`、`--cleanup-helper`、`--cleanup-stage/--wait-pid` 属于稳定的内部引导协议，不应手工用于正常安装。

多文件替换不是整体原子操作。强制断电后需先运行独立更新器恢复，再启动 IDE。不要删除未完成事务的 `.xlang-update`。服务器版本低于本地收据时拒绝自动降级；恢复历史内容需要新的递增发布号。

命令行的 `--url`、`--public-key` 用于明确指定部署/验收来源，不能由在线清单自行更改信任。明文 HTTP 仅在显式 `--allow-http` 且目标为 loopback 时允许；生产要求 HTTPS。始终验证 TLS 主机名和证书，系统信任库不可用时仍有内置 ISRG Root X1/X2。

## 构建与验收

```powershell
python -X utf8 ide/tools/build_update.py
python -X utf8 ide/tools/check_update.py
python -X utf8 ide/tools/check_update_publish.py
```

Linux：

```sh
python3 ide/tools/build_update.py --cc musl-gcc --static --platform linux_x64
UPDATE_TEST_DIR=/tmp/xlang-update-validation python3 ide/tools/check_update.py
python3 ide/tools/check_update_tui.py
```

Python/OpenSSH 是发布机工具依赖，Paramiko 仅用于测试临时 SSH 服务。ARM 使用相应交叉编译器及独立平台名，跨编译可 `--skip-run`；工具仍验证 ELF 的目标架构，不把其他平台产物放入该目录。未实际运行的架构不能视为已验收。

构建记录位于 `ide/build/Release/update/<platform>/build.json`，包含源码/依赖/公钥摘要及产物身份。测试中的 `XLANG_UPDATE_TEST_CRASH_AFTER` 是有界故障注入点，只影响调用者自己的更新进程；正常运行不要设置。

## 来源

- XRT：复用仓库 `lib/xrt/xrt.h`，构建摘要记录实际输入，保留上游单头，不另造网络或 SHA-256 实现。
- LZMA 解码：复用仓库 `lib/lzma/LzmaDec.c`，文件头保留 Igor Pavlov 的 public-domain 声明。
- 制包：从用户的 AutoTestIDE `tools/prepare_update.py` 移植，保留对象格式，扩展管理与推送。
- 内置信任锚：`roots.pem` 包含 ISRG Root X1/X2 公共根证书，取自本机 Ubuntu ca-certificates；证书内容和生成头纳入源码，来源可对照 https://letsencrypt.org/certificates/ 。系统证书库与内置根合并，未绕过证书验证。

IDE 接入代码提供“帮助 → 检查更新 / 更新／修复安装”，后台检查通过独立进程完成，保存/取消沿用正常退出流程；新配置支持缺失/损坏时默认启动。正常 GUI 发布保留引擎/命令帮助匹配校验。本次 GUI 更新沿用既有记录明确接受的旧帮助资料，通过 `publish_update_ide.py` 核对全部受保护运行文件与候选编译输入；这项限定范围的验收不代表完整编译器/命令帮助兼容验收。
