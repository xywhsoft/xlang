# List 与 Array 混合配方

闭合的 `Array<List<T>>`、`List<List<T>>` 和交错嵌套现在使用完整的
XLIR 原生配方。源类型导入和 JSON/XLON 恢复复用驻留事务；Cgen
不实现元素遍历、类型猜测或共享图转换。公开标准库 API、持久化
紧凑 List 格式和已声明的 clone 授权范围保持不变。

## 类型与生成合同

`list-reference-v1` 明确表示独立的原生稀疏 List。其完整身份和
逻辑准入 policy 来自 canonical value-schema，而不是 Array
角色、物理槽布局或 ANY 回退。List 的 Array witness 只携带值域
元数据，不能占用实际容器链的节点或冒充 List 的 owner。

List 与 Array 共用有界的递归预算。跨容器族不能重新计数；恶意
循环类型图、缓存配方篡改、错误身份和不足输出容量都失败关闭。
导入链先完整验证，再一次性发布类型列表；失败不改写调用者输出。

Cgen 沿统一配方只构造一次各层模板，并保留实际 owner 作为事务
seed。List 元数据工厂消费 child witness，只保留不可变 policy，
不保留子容器或 storage。各层 provider 由 IR 容器族明确指定；
语言导入只调用一次共享驻留事务，没有生成的转换循环或 memo。

驻留符号目录新增按需 List schema 组件。构造、投影器、物理槽、
名义准入数据和后代依赖使用同一闭合配方；普通 Array 用例不因
类型池存在 List 行而链接 List 实现。

## 引用与隔离

导入根容器独立于源。经真实原生 owner 与驻留 ANY policy 认证的
擦除子 List 或 Array，按显式目标配方隔离转换。同一来源和配方
深度共用一个结果，完整 int64 稀疏键不缩窄，重复边保持别名。

已匹配完整 typed schema 的子容器保留引用。其 BUSY 状态不要求
读取槽位或转换内容；错误 typed owner、外来 handle、cursor 和
失效 owner 不能靠内容看似匹配获得转换授权。转换拒绝或分配失败
不发布部分结果，也不替换赋值目标的旧值。

JSON/XLON 使用同一类字段与容器图恢复基础，List 仍采用紧凑值序列，
不持久化语言稀疏键。外部共享默认子对象只按声明的 clone 能力
隔离；没有该能力时 checked 拒绝，不修改外部对象。

## 验收与范围

`check_stdlib_list_source.py` 覆盖混合工厂、语言导入、NUL、Unicode、
char、bytes、可空子 List、共享边、错误赋值及循环配方拒绝。
`check_stdlib_list_persistence.py` 覆盖嵌套 List 的 JSON/XLON 文本与
文件恢复、叶域拒绝及实际裁剪产物。

`check_stdlib_list_native.py --restore --import-mutants` 覆盖所有九种
原生叶槽 profile 的 ANY List 转换和已匹配 BUSY 子 List。四个
实际实现负对照必须先编译成功，再由同一行为合同拒绝，包括关闭
原生 List 擦除转换。各分配失败前缀独立关闭资源账本。

验收输入与产物记录在 `E:/xlang-validation/stdlib53-b185-*` 和
Linux 的 `/var/tmp/xlang-stdlib53-b185-*`。完整源码驱动包含新
runtime 组件；Linux ASan/UBSan 只覆盖被选中的原生闭包和生成
程序，不代表整个编译器已进行 sanitizer 验收。

此批补齐上述混合配方及其源导入与恢复边界，不等于容器全集或
全部 53 模块封板。Set/Dict 原生迁移、其余已批准合同和发布产品
总验收仍由长期任务继续完成。
