# 大源文件编辑：单次整形中的字形查找复用

这是 2026-10-10 的第五版私有补丁，实际 SDK17。相对 SDK15，仅修改 `src/xge_text_run.c`；保留前三版文档与 Unicode 优化及第四版 SVG 外层裁剪，总计七个完整源码材料。公开头不变，5,949 个导出名称和序号与 SDK15 一致。新 SDK 尚未安装。

`xgeTextShape` 对至少 256 字节的文本启用 128 槽、2,048 字节的栈上查找表，复用同次调用中重复字符的实际字体与字形编号。散列冲突继续使用原始字体回退解析器；失败解析不缓存，也不改变原有字形输出。表在返回时消失，没有跨调用、字号、DPI、回退链或线程共享的新增状态，不保存文本或字形数组。度量、字距、Emoji 匹配、引用管理与整段整形路径仍按原规则执行。

## 准备与验证

在 `D:\GIT\x-lang\ide` 执行，输出必须是新目录：

~~~powershell
& 'C:\Program Files\Python312\python.exe' .\tools\prepare_shape_lookup_sdk.py --source-root 'D:\GIT\auto_test_ide\build\sdk-candidates\theme-20260916\xge-source' --output '.\build\shape-lookup-sdk-review'
~~~

本轮实际复现全部 237 个材料，并逐个匹配 SDK17；已有目标被拒绝且内容不变，变更来源在创建输出前被拒绝。该入口仅准备源码，不构建或安装。

已有成功的 SDK17 构建报告时，可以编译并运行真实新旧整形比较：

~~~powershell
& 'C:\Program Files\Python312\python.exe' .\tools\test_shape_lookup_sdk.py --build-report '.\build\editor-sdk-shape-v17-build.json' --output '.\build\shape-lookup-equivalence-review'
~~~

入口核对 237 个源材料、两份整形夹具、构建产物和运行时字体。在新目录严格编译一个真实引擎翻译单元，并加入 SDK15 的原始整形函数；诊断程序是唯一 XRT 所有者，不链接 SDK DLL。程序在从未切换到前台的私有 Windows 桌面执行，保留日志、字体来源哈希、编译器栈使用报告与 `result.json`，不在 SDK 中加入测试钩子。

默认读取当前 Windows 的 Consolas 和仓库中的 Noto 字体，也可用 `--latin-font` 与 `--cjk-font` 指定本地字体。Windows 字体可能有 WinSxS 硬链接，入口只读并复制它们；SDK 输入仍保留原有严格检查。Windows 字体不是补丁材料，不随补丁分发。

原始比较和上述落盘入口本轮分别通过 4,801,138 项断言、2,822 组完整整形、3,365,718 项标量查找与 108 组新旧像素对照。比较含字号与回退顺序变化、缓存启用边界、所有 Unicode 标量、缺字、重复／冲突查找、无效 UTF-8、组合字符、中文、非 BMP、Emoji、换行、嵌入 NUL、1 MiB 重复文字与无后端回退循环；像素中包含 12 个明确非空的 ASCII 控制用例。有限字体和测试不代表完整字体、Bidi、GPU 或分配失败验收。

SDK17 整套严格冷构建、三个原始 SDK 测试、六项当前 IDE 回归和 16 组原生文件对照通过。应用回归复用当前程序，在独立目录使用新 DLL；没有执行新 SDK 的应用严格重建、完整标准回归或便携验收。前几版的六份比较材料保留供审查，本轮没有重新执行它们。

超长行响应仍有数秒延迟，内存没有明显改善，性能未验收。具体数据和截图见[本轮记录](../../../docs/large-source-shape-lookup.md)。
