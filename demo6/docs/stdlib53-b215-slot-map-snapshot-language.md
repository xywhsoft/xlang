# SlotMap 独立快照与原生值遍历（B215）

更新日期：2026-10-11

## 本批范围

`SlotMap<T>.keys()` 和 `.values()` 接通公开语言、精确 XLIR 及原生快照。
拥有型原生值的 foreach 接入通用循环合同，不仅服务 SlotMapKey，也适用
已封闭、已准入的原生值计划。供应方 XRT 和已有原生快照算法没有修改。

```xlang
auto entries = SlotMap::<string>()
auto key = entries.insert("你\0好")
array<SlotMapKey> keys = entries.keys()
array<string> values = entries.values()
entries.clear()
assert(!entries.contains(keys[0]))
assert(values[0] == "你\0好")
for savedKey in keys
    assert(savedKey == key)
end
```

## 公开与生命周期合同

| 入口 | 精确结果 | 拥有关系 |
| --- | --- | --- |
| `keys()` | `array<SlotMapKey>` | 独立拥有的键记录；map 修改或销毁后仍可安全比较、传输和释放 |
| `values()` | `array<T>` | 独立快照存储；基础值按值复制，对象／容器仍按受管理引用传递 |

空表返回合法空数组。删除／槽位复用、set／clear 不改变已发布的快照内容。
string 和 bytes 保留精确长度及值语义，包含内嵌 NUL；对象／子容器快照
不是深克隆，修改共享对象仍可见。两次独立调用 **不是成对原子快照**，不能
把一次 keys 与另一次 values 拼成并发一致的映射；本批没有新增并发安全承诺。

原生 NULL 是物理失败，不依赖诊断是否成功分配。快照失败不发布半成品，
失败后原 map 可继续使用。键的生命周期独立于源表，键有效性仍由实例与代际
决定。Set 已批准的“基础值按内容、可变对象／容器按引用身份”规则不变。

## 统一 IR 与生成边界

共享方法表登记 ARRAY_KEY／ARRAY_ITEM 结果角色。Sem 构造精确结果类型；
XLIR 验证独立 Array schema、元素身份、直接 codec、拥有传输和 checked
失败 ABI。Cgen 仅输出 `xl6SlotMapKeysV1`／`xl6SlotMapValuesV1` 调用、
既定布局及不可变 converter，不生成槽表扫描、容器算法或源方法名猜测。

foreach 新增 `native-value-copy` 物理投影合同：借用迭代器载体后通过现有
sealed native-value plan checked 提取独立拥有记录。迭代绑定采用通用
iteration-release；正常推进、continue、break、return、finally 和失败
出口复用已有清理路径。不把原生记录当借用裸句柄或平凡标量，也不宣称这条
通用载体路径已经是无装箱的直接槽位遍历。

## 验收与证据

源码编译器在 Windows／Linux 完整构建 302／301 个翻译单元。后续仅更新
夹具和验收脚本；提交前逐字节复核所有实际编译源码及包含文件与选择索引
相同，不能把未测试的工作区修改一并提交。

| 专项 | 每端结果 |
| --- | --- |
| 快照语言、确定性 C、typed／lowered IR、JIT 和模块 relocation | 通过 |
| 49 个调用、499 个损坏 IR 合同的生成前拒绝 | 通过 |
| 严格 C O0／O2 | 每种优化 1000 次完整源码生命周期，内存平衡通过 |
| keys／values／提取／OOM 完整失败前缀 | 每种优化 4／11／16／274 次；无诊断失败、失败后重试通过 |
| 完整与仅 values 的按需独立程序 | 四份 O0／O2 程序各 1000 次生命周期；仅 values 排除键快照、装箱、哈希和键工厂符号 |
| 真实公共 API AOT | Windows／Linux 实际生成并执行；使用清单展开的物理私有源码 SDK |
| 原生快照专项 | 75 阶段、32 次完整失败前缀及 3 个已编译坏实现拒绝；Linux 选定闭包 ASan／UBSan 通过 |
| Array／List／Set／Dict 原生值与 foreach 联动 | 631 阶段、598 次完整失败前缀、1344 个损坏合同及 5 个坏实现拒绝通过 |
| 原 SlotMap 核心调用 | 337 阶段、318 次完整失败前缀、438 个损坏合同通过 |
| resident 导出闭包 | 263 组件、1398 精确导出、69169 依赖对及 13 个坏登记拒绝通过 |

冻结输入、构建、报告和产物 SHA 位于 Windows
`E:/xlang-validation/stdlib53-b215-*` 与 Linux
`/var/tmp/xlang-stdlib53-b215-*`。旧失败候选保留，不覆盖为成功报告。
最终快照验收命令为 `check_stdlib_slot_map.py --snapshots --pruned`。

AOT 验收使用现有 VFS manifest 的 `%XL6_ROOT%` 私有源码条目，复制后
逐文件校验 SHA，不覆盖不匹配的旧包。它没有重新生成内嵌 VFS，也不包含
独立构建输出中的嵌套编译器资源；不认证 `import xlang` 或发布 DLL／SO。
Linux sanitizer 仅覆盖已报告的原生闭包，不是全编译器 sanitizer。
文档门禁仍有 14 条历史诊断；本批仅要求没有新增诊断。

## 仍待完成

- 同次成对快照／迭代及其公开语言合同。
- 开放泛型、未支持的槽位转换、临时接收者等完整语言边界。
- 拥有型原生值的 nullable／cell／跨挂起 frame 及持久化合同。
- 发布 DLL／SO 与内嵌 VFS、其他容器和三组 53 模块总验收。

`LanguageSlotMapSnapshots=true`、`PrunedSourceExecutables=true`、
`PublicAotExecutable=true`；`LanguageSlotMapFullCutover=false`、
`PrunedShippingArtifact=false`、`Full53Acceptance=false`。DEV-XRT-53
继续进行；CRT 未决策 0、XRT 未决策 19 组及对象池最后不变，见
[标准库决策清单](标准库决策清单.md)。
