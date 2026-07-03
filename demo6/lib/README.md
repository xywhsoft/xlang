# demo6/lib 依赖目录

本目录保存 DEMO6 当前需要随源码一起使用的第三方或基础运行库资产。

## xrt

- `xrt/xrt.h` 是从 `D:\GIT\xrt` 通过 single head 流程同步来的 XRT 单头文件。
- XRT 是 xlang 的标准运行库基础。能由 XRT 提供的文件、时间、内存、字符串、容器、同步、网络等能力，xlang 不应在本仓库重复实现。
- 如果发现 XRT 缺口或 BUG，应优先进入 `D:\GIT\xrt` 修改 `lib` 与 `test`，运行 `singlehead/single_head_maker.exe` 生成新单头文件后，再同步到本目录。

## 维护规则

- 不在本目录临时手改 XRT 单头文件来绕过 xlang 问题。
- package/import/XAPI 测试 fixture 放在 `test/fixtures` 或 `release/extlibs`，不放入 `lib`。
- 标准库能力优先通过 XRT 或 CRT 暴露，只有 XRT 暂缺且不值得进入 CRT 的能力才允许在 xlang 层建立薄补充层。
