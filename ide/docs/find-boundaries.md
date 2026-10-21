# 正则 UTF-8 边界修复

当前开发 SDK67 保留 SDK66 的查找选项修复，并修复空匹配替换的 UTF-8 破坏。C 和 xlang 共用同一实现；既定布局与帮助文档保持原样。

查找 `$`、替换为 `!` 时，只在行尾匹配位置插入一次。`^|$` 可在文本首尾插入。空捕获组 `()` 在中文和 emoji 的 Unicode 标量边界匹配；查找全部、向前与向后查找、全部替换使用一致的标量推进，范围末尾仍保留一次匹配并安全结束。选区内的空匹配不会影响选区外正文，组合字符的各标量完整保留。一次全部替换仍可一次撤销。

“转义”选项中的十六进制字节序列必须构成合法 UTF-8。`\xE4\xB8\xAD\xF0\x9F\x98\x80` 产生 `中😀`；孤立续字节、过长编码、UTF-8 代理项、超出 Unicode 上限和不完整多字节序列均拒绝。普通替换与正则替换共享校验，非法输入保持正文、版本、脏标记和撤销/重做状态不变。已有 `$1`、`\1`、正则 `\0` 分组与整体引用继续可用。

SDK67 只修改 SDK66 的 `src/xui_find.c`，保留 SDK66 的窗口错误提示和失效结果清理。243 份源码材料、119 个翻译单元和 213 项实际预处理依赖均与成套消费者 SDK 绑定。9,081 项公开声明、372 项公开类型和 5,954 个导出保持一致，既有 102 个已冻结 PE 产物的名称导入均可解析，导出序号无变化。[完整 SDK 严格构建](../build/editor-sdk-find-boundaries-051be50ae701/sdk-build.json)、[材料闭包](../build/sdk67-find-boundaries-materials-v1/result.json)、[公开接口比较](../build/find-boundaries-public-compatibility-v1/result.json)。

## 已执行的验收

`find_boundaries_test` 通过 53 项实际 DLL 公开 API 断言，覆盖行首/行尾、空文本、空捕获组、中文、emoji、组合字符、子范围、向后查找、非法字节以及有效多字节转义。[API 证据](../build/find-boundaries-da5b2df6b525/evidence.json)。

`find_boundary_view_test` 通过 100 项实际 C/xlang 窗口断言，点击真实复选框与按钮，验证 EOF、首尾、选区内空匹配和三行标量结果；非法替换不改文档或历史，其他文档和磁盘文件保持原样。八张原生截图已查看。[窗口证据](../build/find-boundaries-17bcc57ac897/evidence.json)。此前的 164 项窗口检查、49 项 API 和 36 个保护页进程在同一 SDK67 上再次通过。[查找选项窗口](../build/find-options-6681316c42a7/evidence.json)、[API 与保护页](../build/find-options-9c76884e662e/evidence.json)。

四个新查找 fixture 已加入普通严格构建和标准完整回归清单。84 模块／103 产物的[严格应用构建](../build/cb-dde2c833/current-build-report.json)、[含渲染的完整回归](../build/find-boundaries-full-v2-40f6f45f/evidence.json)和[实际便携门](../build/find-boundaries-package-native-v2.json)全部通过，构建输入及输出保持一致；三项便携门使用同一精确构建。候选已更新至 SDK67，见[当前入口](../CURRENT-CANDIDATE.md)与[本轮核验](../build/find-options-stage-v1.json)。

此前 SDK66 完整回归的控制器因实时记录文件的 Windows 共享锁而中断；原始证据保留且不计为通过。新控制器用有限次数重试原子替换，临时锁释放后成功，持续锁定则明确报错并保留原记录和临时材料。[中断核验](../build/find-options-full-interruption-v1.json)、[实际共享锁验证](../build/receipt-retry-f6f46a120b43/result.json)。原始空匹配缺陷和解码参数缺失的失败候选均保留，未安装这些私有失败版本。

SDK67 第一次完整回归在中文工作空间的最终渲染启动检查退出 1。注册 fixture 时丢失了测试脚本原有的 UTF-8 BOM，Windows PowerShell 5.1 将路径按本地编码读取；[实际解析器对照](../build/find-boundaries-test-encoding-v4.json)确认恢复 BOM 后的正确中文路径。[首次失败](../build/find-boundaries-full-v1-58c6c327/evidence.json)和第一份未晋升包均保留。新严格构建与前一次的 764 个输入仅 `tools/test.ps1` BOM 不同；完整回归重新实际执行，通过后才验收并晋升第二份包。修改含非 ASCII 文字的 PowerShell 脚本应保留 BOM，以兼容 Windows PowerShell 5.1。
