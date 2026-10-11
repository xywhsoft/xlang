# SlotMap 核心语言 API 接入（B214）

更新日期：2026-10-11

## 本批范围

`SlotMap<T>` 的显式构造和九个核心方法已接通真实源码编译、XLIR、
Cgen 与 resident 原生存储。这是公开核心调用的验收，不是完整 SlotMap
封板，也不是三组 53 个模块的发布验收。供应方 XRT 没有修改。

```xlang
auto entries = SlotMap::<string>()
auto key = entries.insert("a\0b")
auto value = entries.get(key)
assert(value.length == 3)
assert(entries.set(key, "你\0好"))
assert(entries.remove(key))
assert(!entries.contains(key))
```

构造必须明确提供闭合元素类型。接收者沿用现有集合的命名变量／字段规则，
不宣称临时对象链式调用、开放泛型算法或隐式默认构造已经支持。

## 公开合同

| 入口 | 结果 | 正常缺失与错误 |
| --- | --- | --- |
| `SlotMap::<T>()` | 新的 `SlotMap<T>` | 构造失败走 checked 错误边 |
| `insert(value)` | 独立拥有的 `SlotMapKey` | 值必须符合精确 T；分配失败不发布半成品 |
| `get(key)` | T | 空键、过期键或其他实例的键抛 `RangeError` |
| `set(key, value)` | bool | 键不存在返回 false；原生失败走独立错误状态 |
| `contains(key)` | bool | 键不存在返回 false |
| `remove(key)` | bool | 键不存在返回 false；成功删除使原键过期 |
| `reserve(count)` | bool | 非负容量请求；负数抛 `RangeError` |
| `clear()` | bool | 清理存储并使已有键失效 |
| `count()` | int | checked 读取当前元素数 |
| `capacity()` | int | checked 读取容量 |

Key 的复制不借用原 map；map 销毁后 Key 仍可安全释放、比较或传输，但
不能访问已销毁的元素。槽位复用不会使旧 Key 重新有效。这里的 Key 是
SlotMap 的原生值，不改变 Set 已批准的“基础值按内容、可变对象／容器按
引用身份”规则。

已执行的元素用例为 int、int32、精确长度 string、bytes、nullable int、
SlotMapKey、类对象和嵌套 Array。string／bytes 保持值语义；对象和容器
保持引用语义。闭包槽位仍明确拒绝，不能用通用装箱掩盖缺失的转换计划。

## 实现边界

`src/abi/xl6_slot_map_ops.inc` 是 Sem／XLIR 的方法、参数角色、结果角色、
原生符号与物理失败 ABI 的共享登记。XLIR 固定 owner、元素、独立 schema、
准入身份集合、槽位与传输合同，并验证精确操作数及紧随调用的错误观察。

Cgen 只投影已经验证的布局和原生调用：insert 直接传元素槽位地址，get
直接写入精确 T 输出，Key 不经过 xvalue 装箱。生成代码不实现容器算法，
不按源方法名猜测 ABI。组件计划按精确原生符号选择 provider，并为构造及
非构造方法保留各自必需的元素描述符闭包。

`get` 的必需读取和 count／capacity 的 checked size 是 resident 叶子入口。
布尔语言结果与原生调用成功状态独立；原生失败输出为空，不以 TLS 中有没有
诊断推断调用成功。宿主拥有图仅承认登记的 SlotMap 与原生 Key 准入能力，
没有扩大对任意回调／未知描述符的信任。

## 验收

验收使用私有选择索引的完整冻结输入，Windows／Linux 分别重新构建
302／301 个源码翻译单元；不是旧编译器二进制冒充新源码验证。

| 项目 | Windows／Linux 结果 |
| --- | --- |
| 实际源码 JIT、错误捕获、确定性 C、typed／lowered IR | 通过 |
| 严格 C，O0／O2，每种优化 1000 次完整源码生命周期 | 通过 |
| 46 个核心调用、438 个损坏 IR 合同的输出前拒绝 | 通过 |
| 错误 key／value／arity、未支持闭包槽位的源码拒绝 | 通过 |
| 每种优化 insert／get／size／OOM 完整失败前缀 | 11／12／4／132 次，失败后重试与内存平衡通过 |
| 原生拥有值相邻专项 | 双端 77 阶段、86 次完整前缀通过；Linux 选定 native 闭包 ASan／UBSan 通过 |
| 原生值 Array／List／Set／Dict 联动回归 | 双端 479 阶段、446 次完整前缀、1069 个合同变异及 5 个坏实现拒绝控制通过 |
| resident 组件与导出闭包 | 263 组件、1398 精确导出、69169 依赖对；13 个坏登记拒绝控制通过 |

核心调用报告每端 337 阶段、318 次完整失败前缀，并重新校验输入 SHA。
选定 native 闭包的 sanitizer 不是全编译器或本批全部生成代码的 sanitizer。

证据位于 Windows `E:/xlang-validation/stdlib53-b214-*` 与 Linux
`/var/tmp/xlang-stdlib53-b214-*`。最终选择索引、输入／产物哈希和文档门禁
比较须在提交前再次校验。既有文档索引的 14 条历史诊断没有在本批宣称已修复。

## 尚未完成

- 公开 keys／values／成对快照、迭代与相应语言 IR 接线；原生快照已有 B212 基础。
- 开放泛型、未支持的槽位转换及临时接收者等完整语言边界。
- Key 的 frame／cell／跨挂起消费边界与持久化合同。
- 本批 API 的实际独立裁剪产物／AOT 发布验收，以及其他容器与跨组总验收。

`LanguageSlotMapCoreCalls=true`，`LanguageSlotMapFullCutover=false`；
`Full53Acceptance=false`，`PrunedShippingArtifact=false`。DEV-XRT-53 保持
进行中。CRT 未决策 0、XRT 未决策 19 组与对象池最后的顺序不变，完整列表见
[标准库决策清单](标准库决策清单.md)。
