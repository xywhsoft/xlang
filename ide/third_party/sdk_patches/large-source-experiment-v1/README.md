# 大源文件编辑：私有 SDK 实验补丁

2026-10-10 保存的实验源码。它未安装到 `third_party/xge_sdk`，未进入发布程序，性能验收未通过。完整结果见 [大源文件性能记录](../../../docs/large-source-performance.md)。

补丁基于 AutoTestIDE 的固定 XGE 来源快照。`source-materials-before.json` 记录 236 个原始构建材料，`patch-manifest.json` 绑定原始 SDK、来源清单、5 个补丁材料和审核 diff；应用后得到 237 个源码材料。公开头保持原始字节，XRT 仍由 `xge.dll` 单独提供。

## 修改内容

- Unicode 字素类别用只读分页表查询。表重现旧实现的类别，不升级 Unicode 版本或更改字素分段规则。
- Emoji trie 的根节点使用有序索引；下层仍使用原来的链表，保留节点、别名及替换行为。
- Glyph run 按解码后的码点数量预留位置空间。屏幕外绘制使用保守边界；SVG 使用实际绘制边界。
- 文档版本改变时，先清理已经不能命中当前版本的行布局，再生成新布局，避免旧布局占用新布局所需内存。

`files/src` 是实际 SDK9 构建使用的源码，`diffs` 用于审核。C 文件换行统一为 LF，所以 diff 包含换行归一化影响；以清单的原始字节 SHA-256 核对内容。

## 准备独立源码目录

在 `D:\GIT\x-lang\ide` 执行以下 PowerShell 命令。目标必须尚不存在，父目录必须已经存在；脚本不会覆盖现有目录，也不会构建或安装 SDK。

```powershell
& 'C:\Program Files\Python312\python.exe' .\tools\prepare_large_source_sdk.py `
  --source-root 'D:\GIT\auto_test_ide\build\sdk-candidates\theme-20260916\xge-source' `
  --output '.\build\large-source-sdk-review'
```

脚本在写入前检查固定 SDK 和原始材料，生成 `source` 与 `source-preparation-report.json`。本次全新目录复现的 237 个材料与 SDK9 实际构建输入逐个相同；这是源码准备验证，不能代替再次构建 DLL 的验证。现有目标和修改过的来源均已验证拒绝。

需要再次实验构建时，在准备的 `source` 内使用该快照的 `build_dll.bat ide`，并单独保存编译器版本、实际退出码、完整日志及产物哈希。本次已有的三个实验构建使用 GCC 16.2.0，SDK 编译启用 `-O2 -Wall -Wextra`，没有使用 `-Werror`。当前发布 SDK 的编译器版本为 16.1.0，跨版本性能差异不能全部归因于补丁。

## 进入发布前的剩余工作

超长单行仍需优化完整整形、绘制和坐标查询。绘制裁剪需要更全面的 SVG 溢出、字形越界、剪裁边缘与 DPI 回归，缓存修改需要完整 IDE 回归。之后才能审核成套公开头、声明头、导入库、DLL、配置及来源的替换，并重新运行核心严格构建、标准全量与便携包验收。

当前八组原生用例仅证明受限进程内的打开、编辑、撤销、定位和绘制检查完成，不能证明响应速度、实体 IME、混合 DPI、干净 Windows 安装或正式发布条件通过。
