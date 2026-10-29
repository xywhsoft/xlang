# Dict 精确键递归导入与原生恢复

Dict 已具备共享递归导入和 JSON／XLON 原生恢复能力。语言编译器的
Dict 导入／恢复 IR 配方尚未接通；本批不是 Dict 整体完成或发布验收。
Set 仍按已确认规则：基础值按内容，可变对象／容器按引用身份作为键。

## 共享事务与精确键

`XL6_CollectionImportProviderV1` 声明整数键或文本键写入，两者必须
互斥。Dict 使用 `AppendText` 和完整 `xstrview`，不经过整数转换或
`strlen`。空键、内嵌 NUL 和 UTF-8 键保持全部字节。快照持有独立键
和元素引用，原容器修改、释放后仍可使用；不会把槽位地址交给语言。

`xl6DictImportProviderV1` 先认证真实驻留所有者，再观察 typed Dict
槽。语言输入接受真实原生 Dict 或显式 wire OBJECT；原生恢复只接受
OBJECT。外部句柄、BUSY 根、错误完整策略和种子／输入别名不会取得
转换权限。已匹配的 BUSY 子对象保持普通引用，只有真实 ANY 擦除策略
授权嵌套转换；不能根据内容恰好匹配而转换其他 typed 对象。

Array、List 和 Dict 复用一个递归事务及引用映射。同一源对象在同一
schema 域中转换一次，保留原生共享子对象；循环拒绝、失败撤销及种子
消费走原有公共路径。恢复计划复用此提供方，不另建 Dict 恢复器或在
Cgen 写库算法。JSON／XLON 文本不携带引用身份，往返只保证键与值。

无诊断的原生失败在清理前锁定。后续清理错误不替换首个失败，分配、
作用域和模板关闭失败不会发布部分结果。nullable 恢复仅将已证明的
wire 形状／值域不匹配返回 null，原生失败仍是 checked 失败。

## 验证范围

Windows 与 Linux ASan／UBSan 所选原生闭包均通过严格 C11 O0／O2：

- 每端 86 阶段、64 条完整分配／无诊断作用域故障前缀，共 1864 次
  有独立内存账本的故障尝试。
- 空／NUL／UTF-8 键、int64 极值稀疏键、混合容器与同域共享、实际
  JSON／XLON 编解码、原生循环、BUSY、完整策略差异及别名拒绝。
- 无诊断迭代／写入失败、二次清理拒绝、清理错误不覆盖首错。
- 四个真实编译错误实现被拒绝：NUL 截断、禁用共享引用映射、伪造
  typed 擦除权限，以及仅破坏原生恢复的文本键写入。

证据为 `E:/xlang-validation/stdlib53-b200-import-win-dev2/results.json`
及 `/var/tmp/xlang-stdlib53-b200-import-sanitized-linux-dev2/results.json`。
sanitizer 覆盖所选原生闭包，不覆盖整个编译器或 shipping 动态库。

## 后续工作

下一步接通封闭 XLIR Dict 导入／恢复配方并验证生成 C、JIT 和按需
AOT。直接槽优化、资源策略、性能及 53 模块总验收仍继续；不更新
shipping 二进制，不扩展 19 个未决策组，对象池仍最后。
