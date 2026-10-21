# 文档连续片段字素遍历与输入布局定位

2026-10-10，SDK33 将文档 gap buffer 的连续区域交给既有指针字素遍历器，跨 gap 的情况沿用逻辑读取。严格 SDK 冷构建通过；11,645,593 项字素断言、336,188 项文档状态断言、219 项超长字素断言、3 项 SDK 检查、6 项 IDE 检查和 16 组 C/xlang 原生窗口用例通过。8 对完整窗口截图逐像素相同。16 MiB 单行输入仍约 1.8 秒，本候选性能未验收，未安装。

父阶段是[绘制期间裁剪換算复用](large-source-clip-query.md)。237 项源材料仅修改 `src/xui_code_document.c`；公开头、私有文档结构定义和原通用 Unicode 读取器不变，没有新增持久状态、正文副本或堆分配。DLL 的 5,949 个导出名称和序号相同；这不代表完整 ABI 验收。

## 真实调用依据与修改

SDK31 的自有原生进程在 `xuiInputText` 输入后，实际进入 `xuiCodeEditingInsertText → GotoOffset → SelectionClamp → OffsetToLineColumn → xuiInternalTextGraphemeNextRead`。断点核实新文档长度为 16,777,217、offset 从 0 开始、列数输出指针非空。该调用栈证明输入中存在从行头计算列数的路径，不能换算为 CPU 时间占比。

SDK33 的私有 `DocumentUtf8Next` 在 gap 长度为零时使用完整连续文本；offset 位于 gap 后时使用后缀指针并换算逻辑 offset。位于前缀时先调用以 gapStart 为结尾的指针读取，返回位置距 gap 至少四字节时直接使用，否则从原始 offset 重试通用逻辑读取。四字节保护避免末端 UTF-8 解码的有限前瞻受任意字节 gap 切分影响，包括 gap 位于多字节编码中间的人工测试。

存在明确的未关闭成本：一个巨大组合字素或 ZWJ 字素横跨 gap 时，前缀尝试会先走到 gap，再从原 offset 重新遍历。边界结果相同，但工作可能重复；这也是该实验暂不选入便携候选的原因之一。后续需给尝试设置有界预算，或为逻辑 gap 读取增加专用的完整解码实现，并保留相同字素规则。

## 验证与失败记录

[落盘对照入口](../tools/test_gap_reader_sdk.py)复制实际 237 项材料，严格编译两份仅私有改名的完整文档模型、原样 Unicode 模块以及测试程序。30 个实际文档 API 分别改名，SDK DLL 为唯一 XRT 所有者；仅新文档模型启用 next 调用计数钩子。夹具不是对生产 DLL 的修改。

5,708,032 次 next 比较包含七组跨语言、损坏编码与显式 NUL 语料的所有 gap/offset 切分，全部 65,536 种两字节组合，以及确定随机字节与 gap。4,860 次用真实 NOACCESS 页隔开前后片段或在后缀末端放置 guard page，检查有限前瞻不会越界。文档状态回归包含 480 次随机编辑、分组编辑、撤销/重做、版本回绕、回调重入查询、raw/owned 路径、参数别名和查询缓存。

单独的超长字素诊断使用相同已编译 old/new 模型对象与 Unicode 对象，覆盖 524,289 字节组合字符、458,756 字节 Emoji ZWJ 序列以及 393,219 字节 Indic 文本，108 个边界调用、219 项断言通过。组合字符在 gap 靠近结尾时，一次观察为旧 2.364 ms、新 3.273 ms，新路径同时发生一次指针尝试和一次 524,289 次逻辑字节读取。该诊断只说明重复工作的实际发生，不构成整体 IDE 性能基准。

完整 SDK 冷构建实际退出 0、191.906 秒，使用 `-O2 -Wall -Wextra -Werror`、既有三项警告排除及与 SDK31 相同的 GCC 回收间隔参数。SDK Unicode/Emoji/文档三项检查，以及现有 document/editor_view/goto_view/find_view/input/reading 六项检查实际退出 0。应用核心保持既有 83 个对象，不视为完整标准回归或正式发布验收。

三次早期文档夹具构建失败均保留：第一次错误假定 29 个文档 API，在编译前拒绝；第二次 GCC 对测试中 int 长度转 size_t 的潜在负数范围发出 Werror，增加显式长度范围；第三次误将内部 Unicode helper 当作 DLL 导出函数而链接失败，改为编译原样 Unicode-only 对象。第四次四个严格编译/链接步骤及原生进程均退出 0。超长字素诊断首次编译因测试回调的 char/unsigned char 指针签名不符失败，修正签名的新目录运行退出 0，未屏蔽该告警。

## 原生窗口结果

SDK31/SDK33 × C/xlang × 2/16 MiB × 多行/单行共 16 次原生运行，各 45 项检查，合计 720 项。相同 83 个应用对象、固定不闪烁光标、1600 × 1000 私有桌面窗口、1.25 DPI 覆盖和 2 GiB 进程提交上限，未配置语言服务。桌面从未切换。每组合一次观察，宿主负载未隔离。

| 16 MiB 单行 | SDK31 C | SDK33 C | SDK31 xlang | SDK33 xlang |
| --- | ---: | ---: | ---: | ---: |
| 插入一个字符 ms | 1801.605 | 1797.073 | 1740.365 | 1754.894 |
| 撤销 ms | 1835.555 | 1863.449 | 1875.333 | 1693.087 |
| 最大编辑帧 ms | 2461.035 | 2619.104 | 2374.433 | 2370.153 |
| 全流程秒 | 16.313 | 16.390 | 17.907 | 16.563 |
| 最大采样私有 MiB | 977.312 | 976.578 | 993.516 | 992.992 |
| 峰值工作集 MiB | 1403.879 | 1402.961 | 1415.531 | 1414.797 |

16 MiB 多行插入：C 84.126 → 101.519 ms，xlang 58.283 → 56.011 ms；没有稳定的整体改善。独立文档列数转换的单次诊断为 172.799 → 105.415 ms，但包含计数钩子且旧先新后，不能替代窗口结果。插入/撤销时长不包含后续绘制，最大采样私有量不是峰值提交。

两张图已直接查看，C 与 xlang 的状态栏、源文件后缀和源文本正确，左侧命令、编辑器下方单列帮助及右侧上下工作空间/属性保持定版布局。8 对截图不做遮罩或局部裁剪，完整窗口逐像素一致。

![C 单行](assets/native-gap-reader-c-125.png)

![xlang 单行](assets/native-gap-reader-xlang-125.png)

## 布局调用定位

第一次尝试用 GDB Python 计时，调试器实际报告四次“不支持 Python”，没有生成 duration。虽然 GDB 与原生进程退出 0，原始控制器将其状态写成通过是错误的。原报告未覆盖，另存 `build/editor-gap-timed-profile-v1-qualification.json` 明确标记 durationAvailable=false；只保留调用栈价值。

第二次使用外部日志消费者在八个 GDB marker 到达时记录 monotonic 纳秒。前后 marker 包围实际 `finish`，调用栈及 finish 返回点验证插入回到 ProcessTextInput、布局回到 EnsureCaretVisible、后者再回到 ProcessTextInput。PID、持有的进程 handle、创建时间与可执行路径在 attach 前和每个 marker 处验证。GDB detach、原生退出及 43 项探针检查通过。

这一次 C 16 MiB 单行观察：插入 API 125 ms；EnsureCaretVisible 1438 ms；其中 LineLayoutEnsure 1203 ms。这些是包含断点、调试器和管道开销的墙钟间隔，不是 CPU 占比或独立基准。源码同时确认：版本改变会清空旧布局，EnsureCaretVisible 复制整行，再为整段生成 glyph/cluster 数组和全部 caret stops；当前窗口大部分私有内存也伴随这些数组。

后续重点应直接处理长行布局的增量更新与有界保留，兼容原默认 kerning/Emoji flags、标签列、非 ASCII 字素、命中测试、选择范围、换行与 float 按序累加。不得通过切断字素或缩减正文来取得表面改善；任何新方案需与原布局几何及真实像素比较。

## 复现与阶段状态

[第十二版补丁包](../third_party/sdk_patches/large-source-experiment-v12/README.md)保存 8 个累计源材料、8 份 diff、25 份测试和 3 个元数据文件。[源准备入口](../tools/prepare_gap_reader_sdk.py)核验原源库存后输出新目录；237 项输出匹配实际 SDK33，已存在目标拒绝覆盖且保持不变，受控改动输入文档源在创建输出前拒绝。

```powershell
& 'C:\Program Files\Python312\python.exe' -X utf8 tools/prepare_gap_reader_sdk.py --source-root 'D:\GIT\auto_test_ide\build\sdk-candidates\theme-20260916\xge-source' --output build/my-private-gap-source
& 'C:\Program Files\Python312\python.exe' -X utf8 tools/test_gap_reader_sdk.py --build build/editor-sdk-gap-v33-build.json --output build/my-private-gap-replay
```

窗口结果：`build/editor-gap-comparison-v1.json`；文档夹具：`build/editor-gap-durable-v4/result.json`；超长字素：`build/editor-gap-long-cluster-v2-run.json`；外部间隔：`build/editor-gap-timed-profile-v2-run.json`。阶段清单与见证：`build/editor-gap-evidence-cohort-v1.json`、`build/editor-gap-final-v1-witness.json`。本阶段保护此前 935 对冻结材料、83 个核心对象、105 个运行输出、3 个暂存现代编译器输出及便携候选，增加 49 份落盘材料；父清单只用哈希和成功见证衔接，不递归遍历历史构建。

长任务保持 active；正式发布、物理 IME/多显示器 DPI、完整 SDK 发布审查、完整 C 输入图和 ABI 验收等仍未完成。本轮文档读取结果已验证，但性能未验收且存在超长字素重复扫描成本，SDK33 不选入当前便携候选。
