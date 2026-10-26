# 发布构建被 XRT 快照一致性门禁阻断

2026-10-26 从 `demo6` 目录运行 `cmd /c build.bat`，标准库索引和依赖
配置检查通过，`build_dll.bat` 在 `tools/check_xrt_snapshot.ps1` 报
`XRT snapshot metadata is stale` 后退出 1；未生成新的发布版 `xl.exe`。

检查 `lib/xrt/xrt.snapshot.json` 与仓库内资产：

| 资产 | 元数据字节数 | 当前字节数 | SHA-256 |
| --- | ---: | ---: | --- |
| `lib/xrt/xrt.h` | 8,987,779 | 9,002,150 | 不一致 |
| `release/include/xrt_decl.h` | 1,467,090 | 1,470,133 | 不一致 |

两份头文件在工作区中均有并行修改，元数据没有对应更新。不能绕过
一致性检查，也不能只改 JSON 中的长度和哈希来假装源快照有效。

主线同步者应从确定的 XRT 版本重新生成实现单头、公开声明头和快照
元数据，确认三者属于同一版本，再运行 `build.bat` 与最小 `xl run`、
`xl build` 冒烟。普通扩展库任务不修改 XRT/编译器主线；在同步完成前
继续使用现有发布物做包级聚焦测试，但不把它称为新源码的构建结果。
