# C owner 回调：别名隔离与泛型返回转换修复

本轮修复复查报告 `dev/release-materials/provenance/c-owned-review-post-nested-20261101/report.json` 的两项问题。

## 别名保持不变

修复提交：`9d09e136a`。

原实现克隆了函数别名的引用，随后解析别名并通过返回的共享节点修改结果类型。`task.spawn` 因此可能把原来的 `F=func() as cstring` 定义改成普通 string 返回签名，使原回调后续调用发生复制；调用出现在适配之前时还可能造成 IR 签名不一致。cbytes 有相同问题。xcall 另有直接检查别名节点、未解析完整函数形状的问题。

现在对接收端拥有的类型树整体执行规范化。递归过程先克隆解析后的别名定义，再改动副本，涵盖别名链、可空函数和嵌套返回回调。xcall 校验签名前也解析并克隆别名。原 typedef、源函数签名及其他绑定保持不变，包括适配被拒绝的路径。

```xlang
typedef Native = func() as cstring
Native source = reader.Text
string before = source()            // 保持 C 指针
string copied = await task.spawn(source)
string after = source()             // 仍保持 C 指针
var erased = xcall(source)           // 为动态边界适配，原 source 不变
```

回归程序为 `test/cases/extern_owned_callable_alias_integrity.xl`，覆盖类型别名链、普通函数别名、可空流收窄、嵌套工厂、两种语言可变参数、xcall 和 worker。同步路径扫描 256 个分配失败前缀并检查两套分配器的释放平衡。worker 在同步扫描后执行，避免把 await 返回后仍在退役的工作帧计入同步堆快照。

`test/cases/xlir_callable_alias_integrity_contract.c` 检查成功和拒绝路径的别名 AST 形状不变，以及精确 owner 方法调用体没有结果复制。

## 泛型工厂返回转换

修复提交：`5c0f86142`。

泛型方法返回含类型参数的回调时，原语义推断把整个返回形状当作不变类型，因而不能同时推断 `T=c_int` 并允许返回回调的 `cstring -> string` 转换。IR 已有相应的类型化转换路径。

现在仅对返回位置使用递归推断：回调输入继续保持不变，推断结束后闭合类型，再检查返回转换、计入转换成本，并执行泛型约束和最终签名校验。Array 等容器没有获得协变转换，参数模式没有放宽。

```xlang
// Factory<T>(T witness) 返回 func(T) as cstring。
func(c_int) as func(c_int) as string copied = reader.Factory
// 同样可仅根据返回回调的输入类型推断 T。
func() as func(c_int) as string resultOnly = reader.Result
```

`test/cases/extern_owned_generic_callable_results.xl` 覆盖有参/仅结果推断、条件选择、显式转换、字符串与二进制、部分多结果转换、可空及多层回调、可变参数和精确 owner 重载优先级，并扫描 320 个分配失败前缀。四个反例分别验证名义约束、固定输入类型、容器不变性和文本/二进制边界。原生 IR 合同验证闭合参数 ABI 和方法选择证据。

## 成本与验证

两项修复只涉及编译期类型解析、推断和排名，不修改 XRT 布局。原始 owner 回调的读取继续传递原缓冲区，不创建本轮新增的运行时适配层。显式要求普通结果、动态 xcall 和 Future 发布继续沿用既有适配与物化成本；本轮不承诺这些完整调用与手写 C 等速。

完整门禁入口仍为 `tools/check_extern_owned_string.py`，通过 `--runtime-build` 提供源码构建回执后会执行独立 C 消费及 IR 合同检查。验收记录位于 `dev/release-materials/provenance/c-owned-fix-20261102/`，源快照位于 `dev/release-materials/sources/c-owned-fix-20261102/`。

验证使用独立的源码构建；根目录 `release/` 的发布二进制未被替换。
