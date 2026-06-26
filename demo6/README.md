# xlang DEMO6

本目录是 xlang DEMO6 的当前实现目录。DEMO6 的设计目标是把已讨论并落盘的语言基础、C 混编、XRT 集成、标准库、XAPI/import/package、DLL 嵌入式运行等能力压实到发布候选质量。

## 入口文档

- `docs/README.md`：DEMO6 唯一设计口径入口。后续设计、重构和实现默认只参考 `demo6/docs`。
- `cgenrule/README.md`：代码生成规则对照表入口，用于追踪 xlang 写法到 C 代码的生成路径。
- `examples/README.md`：范例目录入口，说明当前可用写法、标准库范例、教学范例和功能套件。
- `test/README.md`：测试资产入口，说明主回归、用例前缀、测试数据和 C fixture 守门。
- `packages/README.md`：package/import/XAPI fixture 入口，说明扩展库系统测试资产。
- `tools/README.md`：工具链配置和守门脚本入口。
- `src/README.md`：编译器源码分片说明。
- `include/README.md`：对外 C API 头文件说明。
- `lib/README.md`：随 demo6 使用的 XRT 单头文件和 package fixture 库说明。
- `release/README.md`：构建产物目录说明。

## 常用命令

```bat
cd /d D:\GIT\x-lang\demo6
build.bat
test_smoke.bat
test.bat
test_all.bat
build_dll.bat
test_dll.bat
```

轻量发布候选守门：

```bat
cd /d D:\GIT\x-lang\demo6
rc0_guard.bat -SkipFullTests
```

## 维护原则

- 语言、ABI、标准库、扩展库、DLL API 等设计变更必须先进入 `docs`。
- 代码生成行为变更必须同步维护 `cgenrule`。
- 新增测试、范例或 fixture 后，应同步维护对应 README 和守门脚本。
- XRT 能提供的能力优先使用 XRT；发现 XRT 缺口时优先在 `D:\GIT\xrt` 修复，再同步 `lib/xrt/xrt.h`。
- `release` 是构建产物目录，不作为源码真相来源。
