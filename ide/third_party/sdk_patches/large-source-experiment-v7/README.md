# 大源文件编辑：两份可见范围结果

2026-10-10 的第七版私有补丁对应 SDK21，基于 SDK17，不包含未采用的 SDK19 度量补丁。相对 SDK17 只改变 `src/xui_code_edit.c`，在同一有效自有缓冲区下保存两份视口端点。新增五个私有整数，没有额外正文或字形副本；本次 GCC Win64 的结构体大小增量实际为 16 字节。

滚动模型和绘制使用不同宽度，原来的单个结果会互相覆盖。两份结果共用原来的文档、版本、读取范围及 Tab 有效条件；外部文本不缓存，复制与申请失败、版本及范围变化继续失效，第三种范围重新遍历。字素端点、Tab 和 Unicode 语义保持原规则。

在 `D:\GIT\x-lang\ide` 执行，输出必须是新目录：

~~~powershell
& 'C:\Program Files\Python312\python.exe' .\tools\prepare_viewport_pair_sdk.py --source-root 'D:\GIT\auto_test_ide\build\sdk-candidates\theme-20260916\xge-source' --output '.\build\viewport-pair-sdk-review'
~~~

本轮实际复现 237 项材料并匹配 SDK21；已有目标被拒绝且内容不变，变更来源在创建输出前拒绝。该入口只准备源码，不构建或安装。

已有成功的 SDK21 构建报告时，可运行比较：

~~~powershell
& 'C:\Program Files\Python312\python.exe' .\tools\test_viewport_pair_sdk.py --build-report '.\build\editor-sdk-range-v21-build.json' --output '.\build\viewport-pair-equivalence-review'
~~~

入口核对 237 项源码、五份实际私有模型／夹具材料和匹配的 SDK 产物，严格编译真实 Unicode 源码与新旧私有辅助函数，使用 DLL 的文档及 XRT API。只在从未切换到前台的私有桌面运行，保留日志与 `result.json`，不修改 SDK 或安装运行时。

原始成功比较与此独立入口各通过 521,973 项断言。预热后 32 次交替查询中，旧模型再次调用字素步进 3,849,696 次，新模型为 0，端点一致。覆盖 Unicode、部分范围、两种宽度、自有／外部缓冲区、版本与文档身份、读取范围变化和受控失败；这是限定场景的复用证明，不代表整个窗口的性能验收。

整套 SDK 严格冷构建、三个原始 SDK 测试、六个现有 IDE 程序和 16 组原生文件测试退出 0，公开头和 5,949 个导出名称及序号与 SDK17 一致。应用没有重新编译，本轮未运行新 SDK 的完整标准回归或便携验收。以前八份比较材料保留供审查，未在本轮重跑。

本轮初版夹具的 CRLF 长度假设导致原生退出 1；修正后原生退出 0，控制器又因结构体大小假设退出 1。第三版验证器核对已有终态日志并通过，随后此入口独立重建、运行通过。全部失败材料保留，详见[本轮记录](../../../docs/large-source-viewport-pair.md)。超长行仍有明显停顿，SDK21 尚未安装或获得完整替换验收。
