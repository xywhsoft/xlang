# 连续字符串字素遍历实验

2026-10-10，SDK29 为连续字符串的 UTF-8／字素向后遍历增加专用路径，减少通用读取回调分派。完整 SDK 严格冷构建、36,483,444 项对照断言、3 项 SDK 测试、6 项相关 IDE 测试、16 组实际窗口用例及独立重放通过。8 对窗口截图逐像素一致；小函数遍历更快，但本轮没有证明整体交互一致改善，不选入当前应用，性能未验收。

上一阶段见[可见字形缓存](large-source-viewport-paint.md)。SDK29 以 SDK27 为父版本，237 个源材料中只修改 `src/xui_unicode.c`，保留已有可见绘制记录上限和默认 Emoji 变更代数机制。公开头保持逐字节一致，实测 DLL 的 5,949 个导出名称和序号相同，仍不能代替完整 ABI 验收。

## 定位与修改

先在现有 SDK27 的自有原生探针中完成两次 GDB 采样。控制器核对自己持有的进程 handle、PID、创建时间和可执行文件路径，两个调试器实际退出 0 并 detach，探针最终退出 0、完成 43 项检查。第一份栈落在 `__xuiUnicodeDecode → xuiInternalTextGraphemeNextRead → __xuiCodeEditVisibleByteRange`，第二份落在 `__xgeGlyphRunDrawPosition → __xgeGlyphRunDrawSpans`。采样干扰了运行，不计入新旧基准；两份栈不能证明各路径的总时间占比，也没有直接采中输入处理调用。

原连续字符串入口仍先处理 NULL、负长度的 `strlen` 和偏移边界。新增的私有 decoder 与字素循环从 SDK27 的实际通用函数体生成，固定字节读取函数，允许编译器直接处理字符串。分类表、Hangul 判定、CRLF、控制字符、Extend／SpacingMark／ZWJ／Prepend、区域旗帜及非法 UTF-8 的原判定保持一致。通用回调 decoder 和 `xuiInternalTextGraphemeNextRead` 的函数体未改动，文档读取回调仍走原路径。

新增路径没有堆分配、正文副本、持久缓存或字体状态。专用和通用循环同源但仍为两份函数，后续修改规则时须同步更新并重跑对照；本轮没有借此改变 Unicode 规则或宣称更完整的复杂文字处理能力。

SDK29 整套严格冷构建使用 `-O2 -Wall -Wextra -Werror` 与既有三项警告排除，实际退出 0，耗时 152.000 秒。源材料前后相同，DLL／导入库／资源对象成套保存。原始 Unicode／Emoji／文档三个 SDK 测试通过；现有 `document`、`editor_view`、`goto_view`、`find_view`、`input`、`reading` 六个程序在新目录使用 SDK29 DLL，各实际退出 0。应用核心没有重新编译，本轮不是完整标准回归或正式便携验收。

## 返回值与读取边界对照

参考为 SDK27 的完整 `xui_unicode.c` 保存副本；旧 API 在独立对象中统一改名。新对象编译实际 SDK29 Unicode 源码，两者共享相同的头及数据表。可执行程序仅导入 SDK DLL 的运行时，没有再实现一份 XRT。比较不向 SDK 添加测试钩子。

首轮和独立落盘重放均实际退出 0，分别完成相同的：

- 36,483,444 项断言、3,496,738 组边界返回值比较。
- 0 至 `0x10FFFF` 全部 1,114,112 个码点值，包含编码后应按原规则处理的 surrogate 值；内部旧日志将该计数称为 `scalar cases`，不能解读为全部有效 Unicode 标量。
- 65,536 种两字节组合，长度 1／2／3 和负值、内部、末尾、越界偏移。
- 278,528 组回调受控读取失败，同时核对新旧返回值与读取次数。
- 8,643 组紧贴不可读内存保护页的长度／偏移检查，覆盖空输入及截断序列；读取保护页会使真实进程失败。

还覆盖中文、组合字符、长组合序列、Emoji／ZWJ、旗帜、Hangul、Indic 字节、CRLF、Tab、嵌入 NUL、负长度 `strlen`、部分字节长度和确定种子的随机非法输入。非法 UTF-8 对照保持旧实现的替换与前进规则，不宣称枚举了所有三／四字节非法组合。

16 MiB 混合字符串的完整步进结果均为 10,969,714 个字素，端点滚动哈希均为 `7abd626d2aaaffe8`。首轮旧／新遍历时间 170.701／93.689 ms，落盘重放为 149.932／90.017 ms。它们是顺序执行的一次小函数观察，包含每步进度断言，不是 IDE 输入性能或稳定加速比例。

## 实际窗口观察

旧／新程序使用同一组已经验证的 83 个应用核心对象，分别链接 SDK27／SDK29。C／xlang × 2／16 MiB × 多行／单行 × 旧／新共 16 次运行，每例 43 项检查，总计 688 项。采用从未切换到前台的私有桌面、1600 × 1000 窗口、1.25 DPI 覆盖和 2 GiB Job 私有提交限制，未配置语言服务。

| 16 MiB 单行实测 | SDK27 C | SDK29 C | SDK27 xlang | SDK29 xlang |
| --- | ---: | ---: | ---: | ---: |
| 插入一个字符 ms | 2263.919 | 1849.666 | 2088.568 | 2184.636 |
| 撤销 ms | 2137.736 | 1823.198 | 1852.215 | 1947.179 |
| 最大编辑阶段帧 ms | 3637.055 | 2753.598 | 2677.895 | 2892.767 |
| 全流程秒 | 22.094 | 18.078 | 19.078 | 19.891 |
| 最大采样私有 MiB | 968.961 | 976.930 | 980.566 | 980.531 |
| 峰值工作集 MiB | 1402.969 | 1403.422 | 1414.742 | 1403.352 |

2 MiB 单行插入：C 241.605 → 219.957 ms，xlang 244.978 → 274.821 ms；16 MiB 多行插入：C 86.453 → 83.719 ms，xlang 61.842 → 54.950 ms。这些方向不一致，宿主机背景负载未隔离，每个组合只有一次观察，不据此把整体性能标为通过。插入／撤销不含随后的绘制，最大编辑帧不含启动；最大采样私有量不是峰值提交，工作集也不是该 helper 自身占用。

8 对截图逐像素一致，已人工查看以下两图。左命令列表、中央编辑器及其下方单列帮助、右侧上下工作空间／属性保持定版布局，中文、组合字符和 Emoji 可见。

![C 16 MiB 单行窗口](assets/native-pointer-grapheme-c-125.png)

![xlang 16 MiB 单行窗口](assets/native-pointer-grapheme-xlang-125.png)

## 重放与保存

[第十版补丁包](../third_party/sdk_patches/large-source-experiment-v10/README.md)保存累计 8 个源材料、8 份审核 diff 和 21 份测试材料。[准备入口](../tools/prepare_pointer_grapheme_sdk.py)只核验固定源材料并输出新目录；本轮 237 项输出逐项匹配实际 SDK29，已有目标拒绝覆盖且内容不变，受控修改输入 Unicode 源后在创建目标前拒绝。没有再次构建 SDK 或安装到应用。

[字素对照入口](../tools/test_pointer_grapheme_sdk.py)核对已完成构建、源材料与两份比较源码，复制未修改的 237 项引擎材料，严格编译两个 Unicode 对象及测试入口，然后在自有进程／桌面中运行。独立重放的三个严格步骤、原生运行和控制器均退出 0，六项计数与首轮一致。复制后的引擎、测试材料和原输入在运行后继续核验。

```powershell
& 'C:\Program Files\Python312\python.exe' -X utf8 tools/prepare_pointer_grapheme_sdk.py --source-root 'D:\GIT\auto_test_ide\build\sdk-candidates\theme-20260916\xge-source' --output build/my-private-pointer-source
& 'C:\Program Files\Python312\python.exe' -X utf8 tools/test_pointer_grapheme_sdk.py --build build/editor-sdk-pointer-v29-build.json --output build/my-private-pointer-replay
```

原生观察在 `build/editor-pointer-comparison-v1.json`，重放在 `build/editor-pointer-durable-v1/result.json`，封存与见证为 `build/editor-pointer-evidence-cohort-v1.json`、`build/editor-pointer-final-v1-witness.json`。本轮保护原有 843 对材料、83 个核心对象、105 个运行输出、3 个暂存现代编译器输出及已有便携候选，父阶段按清单哈希与成功见证衔接，不递归重哈希全部历史 build。

剩余主成本仍包含文档回调字素遍历、完整行布局重建、坐标扫描及首次物理绘制。正式首版的干净 Windows、实体 IME、混合 DPI、完整 SDK 许可及推广、原始 GPU 根回归、完整 C 输入图／ABI、TCC 扩展库和没有 8.3 别名时的深工作目录支持尚未完成。本轮不宣布性能验收、正式发布或长期任务完成。
