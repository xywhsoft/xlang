# C owner 字符串链路：2026-11-01 复查修复

本轮修复上次复查报告中的两项问题，沿用编译期 `cstring/cbytes` 方案，不增加 XRT 类型、对象字段或普通读取分支。

## 重载排序

非空 `cstring/cbytes` 转换到 `string/bytes` 时，继续计算目标可空类型的提升成本。因此 `f(string)` 与 `f(string?)` 并存时，非空实参选择前者；`bytes` 同理。省略默认实参仅在类型转换成本相同后参与排序。别名、方法重载、条件表达式、match 和空值收窄使用同一规则。

修复提交：`c1495775e`。回归程序：`test/cases/extern_owned_value_lift_rank.xl`。

## 返回回调的方法

```xlang
typedef NativeText = func() as cstring
typedef Text = func() as string
class Reader
    func Factory() as NativeText
        return self.Read
    end
    func Read() as cstring
        // 此处返回接管后的 C 缓冲区。
        return cstring.take(nativeText(), nativeFree)
    end
end
Reader reader = Reader()
func() as NativeText exact = reader.Factory  // 保持 C owner 返回 ABI
func() as Text copied = reader.Factory       // 内层回调调用时物化普通 string
```

此前第二种绑定在语义层接受、IR 阶段失败。现在使用有类型的返回值转换，参数类型、参数模式和可变参数包保持原 ABI。可空回调、多返回值、深层回调、泛型工厂、虚方法和接收者逃逸使用相同机制。

修复提交：`b05ab6004`。回归程序：`test/cases/extern_owned_nested_result.xl`；IR 破坏性合同：`test/cases/xlir_bound_callable_result_contract.c`。后者要求不合法的捕获、签名和调用来源在生成 C 或头文件前被拒绝。

### 成本和生命周期

- 保持精确 owner 签名时，不创建本轮新增的返回回调转换闭包，不复制返回缓冲区。已有的接收者闭包、代码生命周期和 C owner 记录仍然存在。
- 明确要求内层普通返回签名时，每个发生签名变化的返回回调创建一个转换闭包，持有原回调引用；调用该回调时按既有规则复制选中的字符串或字节结果。空回调不创建转换闭包。
- 外层方法绑定在自己的调用体中直接完成结果转换，不叠加一个外层转发闭包；转换闭包的入口直接使用其有类型的 bool/out 调用体。
- 不经动态投影或结果装箱，不改变原生 `@owned string` 直通路径。未将此功能表述为整个 OOP 调用与手写 C 等速。

混合 `bytes/cbytes` 普通分支的既有额外 owner 记录不在本轮设计调整范围内，详见 `docs/extern-owned-match.md`。

## 验证入口

`tools/check_extern_owned_string.py --driver <source-driver> --runtime-build <build.json> --directory <fresh-directory>` 执行 JIT、AOT、生成 C、编译反例、独立 C 消费和原生 IR 合同检查。嵌套回调程序逐次检查 384 个分配失败前缀下 XRT 活跃分配和外部分配器的释放平衡，并要求成功和失败样本均存在。

本轮构建回执、结果和源快照索引位于 `dev/release-materials/provenance/c-owned-complete-20261101/`。源码构建验证不代表已重建或替换根目录 `release/` 的发布二进制。
