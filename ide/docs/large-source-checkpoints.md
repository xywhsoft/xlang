# 长行字形检查点与编辑几何复用

2026-10-11，SDK39/SDK41 私有实验，基于 SDK37。保持 C/xlang 双语法及既定布局：左侧命令列表，中央编辑器和中央底栏，右侧工作空间与项目属性。尚未替换固定 SDK 或便携候选；长期目标 active，正式发行和长行性能验收继续。

## 实现

排版检查点保存原浮点相加顺序下的累计值、待完成字形、上一个已输出字形、字体及 Emoji 引用。根据新旧正文的真实公共字节前缀选择检查点，重放边界字形，再生成修改后的后缀。Emoji 匹配可能跨越大量肤色修饰符，因此记录实际向后读取范围及依赖文本结尾的情况，不能用固定数目的前瞻字节代替。字体链、字号、倍率、排版标志、Emoji 参数、默认包与实际包修订变化会使缓存失效。

SDK41 增加可关闭的缓存提供器，DPI adapter 使用逻辑字体转接。当前编辑器仅对至少 4096 字节、没有 tab 的整行复用原光标停止点数组，按检查点重建后缀。含 tab 的行及普通自定义提供器继续原路径。文档、字体、tab 或提供器切换会释放缓存；字体包装销毁前释放其中的 XGE 引用。按字节偏移查询位置改为二分搜索。绘制继续使用原完整段，尚未加入局部绘制。

缓存调用要求回调期间字体和 Emoji 状态稳定。旧缓存被借用，新缓存独立转移给调用方；两者分别释放。普通完整/流式接口的已有回调语义保持。

## 验证

- SDK39v2 完整严格 O2/Werror 构建退出 0，321.187 秒。首次完整引擎比较编译在两个完整构建并行时遇到内存分配失败，原源与日志保留；串行新输出冷编译退出 0，306.562 秒。
- 原始检查点比较包含 63 个用例、16825764 项断言和 6549491 个输出位置，48 次复用、3725502 个前缀字形。覆盖中间插入、追加、中文、组合字符、Emoji/ZWJ/变体选择符、超长修饰符前瞻、嵌入 NUL、无效 UTF-8、2 MiB 浮点累计以及九种字体/参数/资源失效。
- SDK41 完整严格构建退出 0，261.985 秒；原 SDK Unicode、Emoji、document 三项和六个现有应用检查通过。
- 首次编辑器夹具严格冷编译退出 0，271.079 秒，但运行因“中间修改一定有检查点”的测试断言退出 1。实际中文修改点是第 4095 字节，位于首个可用检查点之前；当时新旧几何已一致。修正该测试假设、加入诊断及恢复后的取消用例后，在新目录冷编译退出 0，297.875 秒，原生运行退出 0，6.157 秒。SDK 实现未因这次测试修正而改变。
- 编辑器比较通过 52 组修改/撤销/重做、307152 项断言、104 次恢复和 11394592 次簇回调，逐字节核对全部停止点、段元数据及折行。8 个真实 C/xlang 控件覆盖 100%、125%、150%、200% 的逻辑 DPI、字体与提供器切换、原数组复用和销毁；另有 3 个恢复后取消回调用例及字体先于缓存包装销毁的检查。
- 原 2822 次排版、585 次布局、696 次代理、612 次回调失败、8 个控件生命周期及 108 对 GPU 像素比较继续通过。
- SDK37 → SDK39 的 5951 个导出名称和序号一致。SDK41 增加三个公开提供器接口，自动排序导致 2948 个已有序号变化；完整 ABI 验收尚未完成。

## 实际 IDE 窗口

SDK37 与 SDK41 分别运行 C/xlang、2/16 MiB、多行/单行，共 16 个原生窗口，每个通过 45 条检查。两版共享 82 个应用对象，SDK41 只重编译注册缓存提供器的 DPI/factory 对象；探针源码相同。8 对 1600×1000 完整截图逐像素一致，无裁剪或遮罩。实际显示 DPI 为 125%，关闭 caret blink 用于截图比较。

每格是本机一次观测，后台负载未隔离；采样 Private Bytes 不等于峰值提交内存，不能声明稳定百分比收益。

| 输入 | 插入 ms：SDK37 → SDK41 | 撤销 ms：SDK37 → SDK41 | 最大采样私有 MiB：SDK37 → SDK41 |
|---|---:|---:|---:|
| C / 2 MiB 单行 | 198.759 → 71.604 | 183.376 → 85.549 | 440.965 → 443.227 |
| xlang / 2 MiB 单行 | 175.197 → 63.002 | 170.444 → 62.476 | 443.996 → 445.160 |
| C / 16 MiB 单行 | 1419.466 → 561.779 | 1433.970 → 658.309 | 669.805 → 683.379 |
| xlang / 16 MiB 单行 | 1596.708 → 500.223 | 1528.111 → 521.499 | 683.305 → 699.699 |

16 MiB 单行新候选的最大编辑帧仍为 C 2148.179 ms、xlang 2101.633 ms，最大消息脉冲延迟为 2484/2360 ms。编辑几何生成已有改善，完整绘制仍然明显阻塞；极端长行性能未验收。多行新候选的 16 MiB 插入为 C 84.739 ms、xlang 55.600 ms，也只代表本次观测。

![C 原生窗口](assets/native-checkpoint-c-125.png)

![xlang 原生窗口](assets/native-checkpoint-xlang-125.png)

## 材料与复现

[V15 材料包](../third_party/sdk_patches/large-source-experiment-v15/README.md)保存 86 个文件，包含 14 个累计 SDK 替换及 diff、10 个应用替换及 diff、33 个测试材料、严格构建脚本和描述。准备工具在新目录生成 241 个 SDK 材料及 10 个应用材料，已与实际构建输入逐文件核对。固定 SDK、当前生产程序和用户的 demo6 修改保持原状。

```powershell
Set-Location D:\GIT\x-lang\ide
& 'C:\Program Files\Python312\python.exe' tools\prepare_cursor_sdk.py --source-root D:\GIT\auto_test_ide\build\sdk-candidates\theme-20260916\xge-source --output build\my-cursor-sdk
& 'C:\Program Files\Python312\python.exe' tools\prepare_cursor_app.py --output build\my-cursor-app
# 使用本阶段已完成的严格构建报告；比较输出目录必须不存在。
& 'C:\Program Files\Python312\python.exe' tools\test_cursor_sdk.py --build-report build\editor-sdk-cursor-layout-v41-build.json --output build\my-cursor-test
```

[比较工具](../tools/test_cursor_sdk.py)默认严格冷编译完整引擎；显式 `--reuse-compile-report` 仅在全部 SDK、十份夹具、DPI 源、编译参数、可执行文件和日志匹配后复用。本阶段落盘工具的验收采用同源冷编译产物复用并重新运行检查，未再执行一次落盘工具版本的冷编译分支。所有原生界面检查进程均在自身 Job 和从未切换的私有桌面中运行。

实际报告为 `build/editor-sdk-cursor-layout-v41-build.json`、`build/editor-cursor-layout-fixture-v2/result.json`、`build/editor-cursor-durable-v1/result.json`、`build/editor-cursor-layout-matrix-v1-run.json` 和 `build/editor-cursor-layout-comparison-v1.json`。内存失败与错误测试假设的原始日志也保留。下一步继续处理可见区域绘制及修改后后缀成本，之后才能选入候选；全 SDK 许可、完整 ABI、干净 Windows 和物理 IME/多显示器验收仍待完成。
