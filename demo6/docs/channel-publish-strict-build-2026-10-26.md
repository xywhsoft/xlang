# Channel 发布路径严格编译修正：2026-10-26

完整驻留源复测发现 `xl6_channel_direct_language.c` 与
`xl6_channel_direct_deadline_language.c` 的 `publish` 末尾将条件调用和无条件
返回放在同一行，触发 GCC `-Wmisleading-indentation`。这是核心源码质量
问题，本批修源码，不向新代码添加警告豁免，也不修改测试阈值。

修复只为 `if (!frame)` 的 `failed()` 调用加清晰的括号／换行，`return frame`
仍无条件执行。前面的失败发布、丢弃、错误作用域及生命周期处理完全不改；
没有改变 ABI、XRT、Cgen 或运行时算法。

## 已验证范围

- Windows／Linux GCC，各两个源的 O0／O2：原源码严格编译 8 次均被同一
  警告拒绝；修复后 8 次原严格编译均通过。
- 仅为生成比较基线，将原源码的这一条警告降为 warning；新源码不豁免。
  八组 GCC 修复前后对象逐字节相同。
- Windows TCC 原源码／新源码均编译通过，另外两组对象也逐字节相同。

共 28 次真实编译、10 组相同对象。用不同目录下相同 basename 的真实源，
同一套实际声明头／编译器完成比较，没有替代实现或 provider。
这证明本次格式修正没有改变所测目标的代码／数据，不冒称已执行整个
Channel 运行时测试或重建普通发布产品。

证据位于 `F:/xlang-validation/five-groups-20261026/p7-parameterized/`：
`channel-publish-format-r1/` 与 `verified-channel-publish-format-r1.json`。
旧完整批次使用原不可变快照，仍保留原警告失败；后续复测应使用修复后的
输入，不能把本批结果写回旧报告。

TCC 大参数传输、旧 ABI／IR 宿主迁移、完整参数化和最后联合／嵌套门继续。
目标 active，未决策 CRT 0／XRT 11，内存池最后，无网络／安全性质专项。
