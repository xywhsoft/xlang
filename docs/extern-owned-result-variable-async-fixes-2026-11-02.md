# 已确定的泛型结果与异步回调工厂

本轮修复 `c-owned-review-alias-generic-20261102` 复查中的两类 P2 缺口。

## 复用已确定的类型变量

提交：`62c58b76b`。

`Echo<T>(T value) as T` 的参数已经将 T 确定为 owner 回调时，返回位置现在先使用该绑定，再检查所需的结果转换。语义推断和 IR 泛型闭合使用相同规则，不再试图从普通返回签名重新推断 T。

```xlang
typedef Native=func() as cstring
typedef Plain=func() as string
// identity.Echo<T>(T value) as T
func(Native) as Plain callback=identity.Echo
```

这也适用于 cbytes 和嵌套返回回调。回调的输入类型、参数模式、容器类型及名义约束仍保持原规则；知道 T 不会授权 `array<Native> -> array<Plain>`。

回归程序 `test/cases/extern_owned_inferred_result_variables.xl` 验证复制边界、原始回调指针、条件绑定、显式转换、嵌套工厂及 256 个分配失败前缀。IR 合同同时覆盖原有工厂和新实例，并篡改闭合输入 ABI、选中方法证据，要求在生成 C 前拒绝。

## 在异步 worker 内适配回调结果

提交：`2e4a6a51b`。

异步实例方法返回 owner 回调时，绑定到普通回调的 Future 签名会产生编译器私有 worker 特化。特化在原方法的声明作用域检查，闭合方法类型参数，保留 class 类型参数在模板中的位置，并在发布载荷前完成普通回调绑定。

```xlang
typedef Plain=func(c_int) as string
// reader.Generic<T>(T witness) 返回 func(T) as cstring。
func(c_int) as future<Plain> factory=reader.Generic
Plain callback=await factory(c_int(1))
string value=callback(c_int(7))
```

选中目标后才生成特化；重载探测不会改动方法、别名或已有绑定。原方法仍能绑定到精确 owner Future 签名。现有 Future 值没有获得协变，不能将一个已创建的 `future<Native>` 直接赋给 `future<Plain>`。

虚方法为原 virtual/override 链生成对应的私有槽，保留实际派生类派发。只复制基类方法体会跳过 override，因此不能采用这种简化。

`test/cases/extern_owned_async_callable_results.xl` 覆盖固定/泛型方法、cstring/cbytes、可空回调、可变参数、条件及显式绑定、不同 class 参数、已确定的结果类型变量、虚方法派发、精确 owner 重载优先级及 worker 返回处的分配失败。`xlir_async_callable_result_contract.c` 验证特化 worker 直接发布载荷：本组无 await 的方法不能引入挂起点，也不能调用原异步 worker 来转发 Future。

## 成本与边界

- 不修改 XRT、普通 string/bytes 布局、读取路径或所有权释放 ABI。
- 泛型修复只增加编译期匹配；异步特化不增加转发 Future、等待任务或运行时类型标签。
- 私有 worker 增加生成代码，虚方法特化还增加对应槽的静态元数据。这些是特化的代码和冷数据成本。
- 绑定继续承担原有闭包成本；返回已有 owner 回调时，普通签名仍需结果适配闭包。通过普通签名调用生产者，继续执行约定的载荷复制；精确 owner 签名保留原指针。本轮不宣称整条异步调用与手写 C 等速。

完整门禁为 `tools/check_extern_owned_string.py`，独立 C/IR 门禁为 `tools/check_extern_owned_string_public.py`。验收回执和源码快照分别位于 `dev/release-materials/provenance/c-owned-fix-result-20261102/` 与 `dev/release-materials/sources/c-owned-fix-result-20261102/`。验证使用隔离的源码构建，不替换发布版编译器二进制。
