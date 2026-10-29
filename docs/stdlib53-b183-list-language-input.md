# List 稀疏输入与共享导入事务

List 的驻留导入事务现在明确区分语言输入与持久化输入。语言输入
保留完整的 int64 稀疏键；JSON/XLON 的紧凑数组格式仍恢复成连续键。
两种模式不能混用，也不会在类型拒绝后回退到另一种输入格式。
本批补齐普通工厂和 cast 迁移所需的底层输入协议，不增加公开 API。

## 输入和所有权

语言模式只接受真实 wire int-map 或经实际 handle ops 认证的原生
List。TypeId 相同的其他 handle、普通数组或伪造的目标模板均拒绝。
原生输入直接读取受保护的 typed 槽位，不构造临时 int-map 或容器
克隆；标量使用真实转换器，引用子对象保留其实际身份。

输入快照分别拥有每个元素和完整键。读取结束前不交出物理槽位，
结束后不保留源对象的 BUSY pin；修改或释放源对象不使快照失效。
源对象不会被导入事务 retag 或改写。成功返回独立的根容器，
原生子对象沿用普通浅复制语义，不被隐式深复制。

混合 Array/List 的 wire 输入复用原有的一个类型深度 memo。
同一来源在同一 schema 深度的重复边只转换一次，不按键重复复制
子图。每层必须由明确的提供者和完整准入域授权，不能以 ANY
兜底；真实原生子对象仍须符合目标的完整域。

## 错误和依赖

所有有效的 seed 表在成功和失败路径均恰好消费一次。输入 Begin
即使失败，也可交出部分清理记录，由 End 统一退休。无诊断的 false
结果在清理前锁定为首次 checked 失败，不能被后续析构错误替换。
正常操作中的清理错误则作为操作失败交付，不发布部分结果。

List 输入组件显式依赖通用输入清理组件。生产源、JIT 与独立产物
沿用同一驻留目录，没有 Cgen 逐项导入算法或调用者补提供者的兜底。
已有恢复模式的叶值归一化和四种持久化格式保持原合同。

共享默认子对象继续只按声明的 clone 能力隔离，否则 checked
拒绝；本批输入协议不扩大 clone 的授权范围。此前合同见
[List 的 JSON/XLON 投影与恢复](stdlib53-b182-list-persistence-native.md)。

## 验收

Windows 和 Linux 的选定原生闭包各通过 1,220 个阶段、84 条完整
故障前缀及 5,366 次逐点分配尝试。每次尝试独立核对资源账本，
批量执行只减少进程启动成本，不跳过故障点。覆盖内嵌 NUL、UTF-8、
bytes、char、nullable、整数和浮点域、INT64 两端键、共享混合图、
大输入、输入快照退休、错误模式拒绝和诊断优先级。

三项真实实现负对照分别把键窄化、移除首次失败锁定和补造 Append
内存错误；均先严格编译，再由同一组合同拒绝。Linux ASan/UBSan
仪器化选定原生
输入闭包，不认证完整编译器或生成模块的全部代码。

原生报告：

- Windows：`E:/xlang-validation/stdlib53-b183-native-win-final2/results.json`
- Linux：`/var/tmp/xlang-stdlib53-b183-native-linux-final2/results.json`

Windows 272 个、Linux 271 个编译单元从选定源码新编译，不使用
对象覆盖或旧 shipping 实现代替本批代码。List 持久化、Closed/Open 恢复、开放复制、
DynamicFields、List 方法与代数六项相邻回归分别通过 234、96、167、
104、84、126 阶段，双端阶段数相同；各端驻留目录通过 233 组件、
1,223 精确导出和 13 负对照。正式回归证据：

- Windows：`E:/xlang-validation/stdlib53-b183-regressions-win-final2`
- Linux：`/var/tmp/xlang-stdlib53-b183-regressions-linux-final2`

产品、源码、fixture、runner 和负对照在最终提交门重算哈希。
公开能力矩阵与 deferred API 清单逐字不变；文档门保留相同 14 项
历史拒绝，本批新增为零，不作为全仓文档门通过。

## 后续迁移

普通 List 字面量、默认构造和 cast 的 XLIR/Cgen 入口尚未切换到
本批协议。封闭的 `Array<List<T>>`、`List<List<T>>` 构造 recipe、
成员表达式双变量遍历、Set/Dict、发布 DLL/SO、VFS、打包 AOT 和
三组整体总验收仍待完成。原生输入通过不等于这些语言入口已迁移。

CRT 未决策为零；XRT 19 组未决策及对象池最后的安排不变，见
[标准库决策清单](标准库决策清单.md)。
