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

## 同日复验：两组快照均失配

当前再次运行 `tools/check_xrt_snapshot.ps1`，仍在 XRT 元数据校验处
退出 1。四份头文件与两份元数据在工作区均无未提交修改，当前检出
状态已失配；不能继续把原因限定为并行任务尚未提交的改动。逐项核对：

| 资产 | 元数据字节数 | 当前字节数 | 当前 SHA-256 |
| --- | ---: | ---: | --- |
| `lib/xrt/xrt.h` | 8,987,779 | 9,003,471 | `7CD90D9F2BF3F2B92EF6220D8D6D9C8C6AC7A1A3CF044711739A39628CDAF0FF` |
| `release/include/xrt_decl.h` | 1,467,090 | 1,470,275 | `A8B3999AB5BF787CCC953DD8BD81D807CE2416B355FB4897C64130461D6A1186` |
| `lib/xruntime/xruntime.h` | 9,668,234 | 9,683,926 | `8B2091A4C9D9B00C5747C709F3942B3D0044A7E8A7AE4491D39B5BA85B2559DE` |
| `release/include/xruntime_decl.h` | 1,616,627 | 1,619,812 | `4FEC9524A4EC86D988D28A69623FF69D8559047F77A405A930C674654FDBF06F` |

主线同步时须同时处理 XRT 与 xruntime 的实现头、声明头和各自元数据；
只修首个报错的 XRT 快照会让构建在下一组校验继续失败。
