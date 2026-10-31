# release/tmp 来源核查

2026-10-31，核查对象为根发布目录 `D:\GIT\x-lang\release\tmp`。检查时它是空目录，没有运行中的 IDE 或语言服务。

## 来源

1. 上轮图标／字体验证脚本 `ide/build/Release/icon-font-20261031/verify.py` 显式创建了 `release/tmp/icon-font-20261031`。证据已迁到本轮构建目录，但空 `tmp` 父目录没有删除。这是本次检查时空目录的来源。
2. `tools/check_cli_temp_cleanup.ps1` 原先先创建 `release/tmp`，末尾仅检查它为空；现已取消预创建，并在本轮引入的父目录为空时非递归删除。
3. `tools/clear_release_tmp_files.ps1` 原先只删除目录内容；现已补齐空父目录清理。
4. 引擎 `src/driver/xl6_xlc_shell.c` 仍按需使用 EXE 旁的 `tmp`：非 JSON 的文本 `check`、`--save-temps` 和向标准输出导出 C 的内部文件路径。仅计算默认路径不创建目录，普通编译默认不保留临时文件。此轮保留引擎现有策略。
5. IDE 的快照／深路径启动暂存使用系统临时目录。C LSP 代码仍由宏关闭；其保留代码也使用系统临时路径。

## 实际验证

使用根 `release/` 的既有 IDE、语言服务、SDK 和引擎，先移除空父目录，再逐项检查。程序、用户设置的摘要保持不变。

| 操作 | 退出状态 | 结束后 release/tmp |
| --- | --- | --- |
| IDE 启动，实际渲染 | 0 | 不存在 |
| IDE 打开 xlang 文件，30 帧 | 0 | 不存在 |
| IDE 打开 C 文件 | 0 | 不存在 |
| CLI 普通 C 编译 | 0 | 不存在 |
| CLI 普通 xlang 编译 | 0 | 不存在 |
| CLI `check --json` | 0 | 不存在 |
| CLI 文本 `check` | 0 | 存在，内容为空 |
| CLI 临时文件清理回归，包含 `--save-temps` | 0 | 不存在 |
| 清空脚本处理空目录，再处理不存在的目录 | 均为 0 | 不存在 |

首次 xlang 编译探针将 C 与 xlang 的 `main` 放在同一目录，被自动 C 辅助文件发现机制一起编译，报入口重复；分开放置后通过。原失败保留，不将它归因为临时目录问题。

原始日志和结果保存在 `ide/build/Release/tmp-audit-20261031/`。本轮没有修改 IDE 或编译器程序，无需重新编译。

日常验证仍运行根 `release/` 程序，日志和夹具默认保存到 `ide/build/Release/<本轮目录>/`；不为验证材料创建发布目录中的 `tmp`。专项测试清理自有内容后，也必须清理本轮新建的空父目录。
