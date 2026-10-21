# 大源文件编辑：文档坐标缓存实验

这是 2026-10-10 的第三版私有 SDK 补丁，实际构建为 SDK13，尚未安装到 IDE 的发布 SDK。它保留第二版的五个材料，仅新增 `src/xui_code_document.c` 的坐标转换缓存。公开头不变，XRT 仍由 `xge.dll` 单独提供。

每个文档保留一个版本绑定的转换结果，六个 32 位字段共 24 字节，不增加文本或字形副本。正向查询保存实际字素列及对应的规范字节位置，反向命中返回规范位置，保留输入位于 UTF-8／字素内部时的原有语义。只查询行号的缓存未命中不会覆盖完整结果。底层正文替换前先使缓存失效，覆盖同版本中间状态、撤销／重做和版本回绕。

实际新旧文档模型完成 336,188 项等价性断言；重复正反转换及其间的行号查询没有再次调用字素扫描。第三版保存了四个已运行的 C 测试材料，旧模型与新模型各完整保留，公共函数分别改名为 `old_` / `new_`；两者使用相同的真实 Unicode 实现和单一 XRT。测试含非公开修改辅助函数和扫描计数器，仅用于比较夹具，不会编入 SDK。

## 准备源码与复查

在 `D:\GIT\x-lang\ide` 执行，输出必须是尚不存在的目录：

~~~powershell
& 'C:\Program Files\Python312\python.exe' .\tools\prepare_position_cache_sdk.py --source-root 'D:\GIT\auto_test_ide\build\sdk-candidates\theme-20260916\xge-source' --output '.\build\position-cache-sdk-review'
~~~

入口复用原始来源、完整材料和新目录检查。它只准备 237 个材料，不构建或安装。`patch-manifest.json` 绑定六个完整补丁材料、审核 diff 和四个等价测试材料。本轮重新准备的全部材料逐个匹配实际 SDK13 构建输入，已有目标及变更来源也实际拒绝。

已有成功的私有 SDK 构建报告时，可以运行等价测试；示例使用本轮报告：

~~~powershell
& 'C:\Program Files\Python312\python.exe' .\tools\test_position_cache_sdk.py --build-report '.\build\editor-sdk-position-v13-build.json' --output '.\build\position-cache-equivalence-review'
~~~

测试工具复核 237 个源材料、四个夹具及构建报告绑定的 DLL／导入库，在独立新目录严格编译并运行两个模型，保存原始日志与 `result.json`。它不会构建整个 SDK，也不会修改应用。该落盘入口本轮实际运行成功。

SDK13 冷构建、修改的 Document 单元严格编译、三个原始 SDK 测试、六项当前 IDE 程序回归与 16 组原生文件对照均通过。整套 SDK 构建没有使用 `-Werror`；IDE 回归复用当前可执行文件、替换独立测试目录中的 DLL，没有重新编译应用，也没有执行新 DLL 的完整标准回归或便携验收。

超长行仍有数秒编辑延迟，性能未验收。测试没有注入内存分配失败，也没有解决原有底层修改的分配失败事务问题。有限截图不证明所有字体、Bidi、裁剪或 DPI 场景。详细时间、截图及首版剩余边界见[本轮记录](../../../docs/large-source-position-cache.md)。
