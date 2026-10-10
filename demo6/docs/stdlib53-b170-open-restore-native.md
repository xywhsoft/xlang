# Open 动态字段恢复事务

共享默认子对象只通过声明的 clone 能力隔离；没有能力、返回自身或结果仍
被外部共享，均为 checked 失败，不能修改原对象。Open 恢复的原生事务
底座已按这一规则实现。语言层 Open 描述表、clone 适配器与实际入口仍待
切换，现有 Open pending 标记保留，本批不代表标准库或发布总门完成。

## 事务与所有权

私有 Open 描述保存存储类的名义身份与 runtime tag、父存储字段名、子
handle 字段名、静态字段的线格式及运行时键、声明的 clone 入口。字段名
全部按精确字节跨度处理。忽略字段、别名、存储字段和 runtime tag 不被
写入动态字段表；非法描述在构造和 nullable 输入拒绝前作为操作错误拒绝。

驻留 provider 先认证默认存储的真实类寿命及 handle Ops，再读取 payload。
即使输入没有额外字段，伪造标记、错误 tag、缺失 handle 和外来策略也不能
成为合法默认对象。任意原生 C 指针仍由显式 FFI 调用者负责其有效性。

需要恢复额外字段时，在所有拥有图冻结区之外调用声明的 clone。用已有
XRT 字典算法创建新的字段表，保存 clone 默认字段的实际拥有边，并追加
线格式额外值。额外值沿用已有 erased Value 复制语义，不赋予未知资源
新的深复制或 clone 能力。已观察的线格式 store 保持快照，clone 回调
对原输入 store 的修改不替换本次恢复所用的键和值。

新字段 handle 的替换与父对象存储槽替换，都进入现有原子字段提交记录。
整批提交前检查实际槽隐私和完整物理拥有图；不先修改 clone 的旧原生
payload，再判断 clone 是否共享。外部泄漏的 clone 被拒绝后，其原字段
表也保持不变。父对象仍共享时复用声明的父 clone 与整图重新准备，不
再运行初始化器。诊断对象和无诊断失败位都参与原有首错与清理事务。

## 组件边界

新增可选 RESTORE_OPEN 驻留组件和一个精确 provider 导出。它依赖现有
类准入、DynamicFields 核心、原生 handle 及 checked 错误组件，不因自身
依赖拉入 DynamicFields 图复制组件。复制测试中的图复制能力来自测试
声明的 clone，而不是额外授予所有类的结构复制权。

闭合类 provider 保持独立；没有新增标准库公开 API、XRT 算法或 Cgen
库函数体。私有恢复节点增加可选数据描述，既有节点默认关闭该能力。

## 验收

最终冻结输入为 `stdlib53-b170-source-final1/frozen/demo6`，Linux 为同一
index 的镜像。Windows、Linux 分别无缓存重建 257、256 个源码翻译单元。

- `open-win-final1` 与 `open-linux-final1` 各 12 阶段通过。O0/O2 验证
  精确 NUL 与 UTF-8 键、静态别名和忽略字段、共享父 clone、共享/自身
  子 clone 拒绝、回调修改已观察输入、无诊断失败和物理策略拒绝。每个
  优化级完整执行 71 个连续分配失败前缀，止于首个无故障尝试；原共享
  对象不变，分配和释放账本平衡。Linux 仪器覆盖所选原生闭包的
  ASan/UBSan，不是整个编译器或发布库的 sanitizer 验收。
- 三个实际 provider 变异分别漏过滤静态键、漏追加额外值、提前写入
  receiver，均先通过严格编译，再由真实合同断言拒绝。
- `restore-*-final1` 各 59 阶段、`resident-*-final1` 各 82 阶段通过，
  包括实际 TCC JIT 和 O0/O2 生成 C 执行及平衡账本。九个实际 IR 破坏
  在生成 C、头和组件计划前拒绝。Windows 十二份生成 C 与 B169 逐字节
  相同，证明没有把原生底座验收冒充语言 Open 切换。
- `catalog-win-final1` 与 `catalog-linux-final1` 的 JSON 报告各覆盖
  218 个独立组件、1,179 个精确导出和 13 个拒绝负控。源码、产品和
  最终报告输入通过双端最终 index 哈希核验。

上述证据位于 Windows 的 `E:/xlang-validation` 和 Linux 的 `/var/tmp`。
早期 dev 报告保留但不作为通过证书。本批没有重新构建或提升发布 DLL/SO、
VFS、AOT 裁剪产物，也不计为全部语言 OOM 前缀或全仓 RC0 通过。

## 后续工作

下一步由 XLIR 推导并验证 Open 描述、全部静态键和声明 clone 合同，再让
Cgen 只发出数据表与既定 ABI 适配。实际语言恢复迁移后才撤销相应 pending
标记。Set/List/Dict 等其余恢复载体、资源策略、性能和发布总门继续。
CRT 无未决策，19 个 XRT 未决策组及对象池最后不变，见
[标准库决策清单](标准库决策清单.md)。
