# demo6/lib 依赖目录

本目录保存 DEMO6 当前需要随源码一起使用的第三方或基础运行库资产。

## xrt

- `xrt/xrt.h` 是 DEMO6 构建使用的唯一 XRT 单头文件，由当前 XRT 主线的
  `single/xrt.h` 生成并通过 `tools/sync_xrt_snapshot.py` 同步。
- XRT 是 xlang 的标准运行库基础。能由 XRT 提供的文件、时间、内存、字符串、容器、同步、网络等能力，xlang 不应在本仓库重复实现。
- 如果发现 DEMO6 所用 XRT 的缺口或 BUG，应优先在 XRT 主仓修改源码、测试、文档与模块清单，通过 XRT 成熟度检查后再执行同步。
- `xrt/xrt.snapshot.json` 同时固定实现单头与 `release/include/xrt_decl.h` 的哈希、字节数和公共函数数；`src/api/xl_xrt_symbols.inc` 由同一实现快照生成，作为 TCC 运行时符号的唯一清单。

## 维护规则

- 不在本目录临时手改 XRT 单头文件来绕过 xlang 问题。
- 使用 `python tools/sync_xrt_snapshot.py --sync --xrt-root <path>` 同步，使用 `tools/check_xrt_snapshot.ps1` 检查仓库内快照。
- package/import/package 测试 fixture 放在 `test/fixtures` 或 `release/extlibs`，不放入 `lib`。
- 标准库能力优先通过 XRT 或 CRT 暴露，只有 XRT 暂缺且不值得进入 CRT 的能力才允许在 xlang 层建立薄补充层。
