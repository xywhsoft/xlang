# 真实扩展库的 C/xlang 分析与导航

本增量修正随 IDE 提供的私有编译器资源和声明来源映射。界面继续使用定版布局，C 与 xlang 使用各自语法；没有增加首版以外的业务面板。

## 修正

Win64 TCC 的 `stdio.h` 在普通配置下先声明外部 `vsnprintf`，随后以 `static inline` 定义同一函数。TCC 可构建该程序，clangd 则报告静态／非静态声明冲突。私有资源现在只在 `__NO_ISOCEXT` 配置下保留前面的外部声明，普通配置继续使用原来的内联实现；没有替换 IDE 缓存中的头字节或屏蔽诊断。内嵌资源由编译器完整源码构建生成，分析会读取该编译器实际报告的头内容。

此前真实 C 原型生成器只为导出函数、全局和名义类型提供来源绑定。可达的外部扩展函数虽有准确原型，F12 仍指向 IDE 私有临时头。现在共享原型生成器可在写出标识符的位置报告已有 lowered 符号身份，头生成入口将该身份送入原有 AST 来源／范围校验。普通 C 实现生成和不请求映射的头生成继续使用同一原型字节；映射不通过名称反查声明。运行时桥接函数仍由其实际组件负责；没有可验证来源的条目保留空目标。

IDE 现有来源守卫、未保存版本、范围和项目归属检查继续应用。扩展库磁盘声明绑定版本为 0，并保存原始文本大小和 SHA-256；跳转前仍须验证实际来源。

## 实际验收范围

回归工具 `tools/check_c_extensions.py` 为 CRC32 和 MD5 各建立 Unicode 路径中的真实 xlang+C 工程。每项使用随编译器提供的扩展包和实际 native 头，不添加工程头替代品；验证 xlang 调用、C native 调用、共享导出、`vsnprintf` 函数指针调用和 `snprintf` 执行。

同一编译器用于 IDE 真实后台 C 会话：诊断为空，悬停和补全来自真实后端，F12 绑定到导入扩展的 xlang 声明，修改已读取的间接 native 头会使会话失效并拒绝继续请求。工具只修改自有副本，恢复后核对原提供者和测试输入摘要。独立编译器查询还验证真实声明来源身份、选择范围和普通头生成字节一致。

初始 CRC32 失败诊断、临时头导航失败、实验性工程头覆盖及早期夹具失配均保留于 `build/c-extension-probe-v1-*` 至 `v3-*`；第四版使用完整重建后的真实编译器资源通过。早期失败不计作产品通过。

此次只验证这两个扩展及上述调用形式。`cPreprocessorGraphComplete` 和 `abiEquivalenceGuaranteed` 继续为 false；复杂属性、其他调用约定、跨 DLL 聚合和更多扩展尚需覆盖。干净 Windows、实体 IME／混合 DPI 与正式发行条件继续验收，长期目标保持 active。

最终严格构建与含渲染完整回归均已通过；便携候选另做包内验收后更新当前入口。

- [严格源码构建](../build/cb-6bcdf568/current-build-report.json)
- [完整回归](../build/c-extension-full-v2-e2f3f9e2/evidence.json)
- [编译器完整源码构建](../build/compiler-c-native-extensions-v1/build-extensions.json)
- [三张已查看原生截图](../build/c-extension-visual-review-v1.json)


本轮便携与最终材料：

- [十项包内验收](../build/c-extension-package-native-v1.json)
- [最终核验](../build/c-extension-delivery-final-v1.json)
- [保留的失败与有限实验](../build/c_extension_failure_notes_v1.json)
