# 大源文件编辑：字形度量复用实验

2026-10-10 的第六版私有补丁对应 SDK19，相对 SDK17 只改变 `src/xge_text_run.c`。实际语义、绘制和相关回归通过，但未证明整体性能收益，不选作安装候选。

调用内表为 64 槽、2,048 字节，复用字体／字形编号及整形消费的前进宽度、右边界。失败度量不缓存，冲突重新解析；不保留跨调用状态或新增文本／字形数组。公开头不变，5,949 个导出名称和序号与 SDK17 相同，尚未完成完整 ABI 验收。

在 `D:\GIT\x-lang\ide` 执行，输出必须是新目录：

~~~powershell
& 'C:\Program Files\Python312\python.exe' .\tools\prepare_shape_measure_sdk.py --source-root 'D:\GIT\auto_test_ide\build\sdk-candidates\theme-20260916\xge-source' --output '.\build\shape-measure-sdk-review'
~~~

本轮实际复现全部 237 项源码，并匹配实际 SDK19；已有目标被拒绝且内容不变，变更来源在创建输出前被拒绝。该入口只准备源码，不构建或安装。

已有成功的 SDK19 构建报告时，可运行真实新旧比较：

~~~powershell
& 'C:\Program Files\Python312\python.exe' .\tools\test_shape_measure_sdk.py --build-report '.\build\editor-sdk-measure-v19-build.json' --output '.\build\shape-measure-equivalence-review'
~~~

入口核对 237 项源材料、两份本轮夹具、构建产物和本地运行时字体；在新目录严格编译真实引擎，并包含 SDK17 的原始 128 槽查找表、解析器和整形函数。诊断程序是唯一 XRT 所有者，不链接 SDK DLL，在从未切换到前台的私有桌面运行，保留日志、栈使用报告和 `result.json`。

默认使用本机 Windows Consolas 与仓库 Noto 字体，可通过 `--latin-font` 和 `--cjk-font` 指定其他本地输入。Windows 字体只读复制并记录哈希，允许系统字体的 WinSxS 硬链接，不放入补丁；SDK 材料保留严格检查。

原始夹具和此入口各实际通过 14,809,718 项断言、2,822 组完整整形、3,365,718 项标量查找、6,672,384 次实际度量对照和 108 组像素比较。包含 12 组非空像素控制、三组失败度量重试控制、字号与回退顺序变化、缓存边界、Unicode 标量、组合字符、Emoji／ZWJ、非法 UTF-8、嵌入 NUL 和重复文字。有限字体及策略不能证明所有复杂文字、GPU、并发字体修改或分配失败路径。

整套 SDK 严格冷构建、三个原始 SDK 测试、六项现有 IDE 程序和 16 组原生文件对照退出 0。应用未重新编译，本轮没有执行新 SDK 的完整标准回归或便携验收。前几版的八份比较材料保留供审查，本轮没有重跑它们。

七份完整源码与七份审核 diff 是累积实验材料；两份本轮夹具连同此前八份比较材料保存在 `tests`。具体测量、准备器失败修复、截图、新调用栈发现和后续工作见[本轮记录](../../../docs/large-source-shape-measure.md)。当前应用、固定 SDK 与便携候选不变。
