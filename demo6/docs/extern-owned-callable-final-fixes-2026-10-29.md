# C 字符串链路：可空与可变参数方法绑定

修复 `build/owned-string/review-boundaries-107/report.json` 中的三项问题。
基于主线 `54244a54c` 在隔离工作树实现；同步 `6eb362b14` 时编译器构建输入
没有变化。普通字符串运行时和 XRT 均未改动。

| 提交 | 行为 |
| --- | --- |
| `7a146043a` | 泛型类方法接收可空函数参数时，直接方法组、非空函数及 null 在具体调用边界正常校验 |
| `56b270868` | 实例方法可直接转换为可空函数类型，包含别名、泛型、合法 private 方法和异步结果 |
| `fde3d3932` | `...` / `***` 实例方法可绑定到 cstring/cbytes 及普通 string/bytes 返回签名 |

## 编译与使用

泛型推断只处理仍需推断的类型参数。形参类型已由类参数或前序证据完全确定时，
推断阶段不再把普通函数到可空函数的提升误认为类型参数冲突；具体调用仍校验
全部实参和 ABI。IR 篡改测试要求把合法回调换成标量后在输出 C/头文件前拒绝。

可空方法转换 `object.Method as F?` 在语义阶段规范化为
`(object.Method as F) as F?`。内部使用已有精确绑定，外部使用已有指针提升。
接收者只求值一次，只构造一个闭包；访问限制和泛型约束不能通过可空转换绕过。

可变参数方法示例：

```xl
class Reader
    func Text(int mode=7,...) as cstring
        // 此处按原有合同取得 C 缓冲区并交给 cstring.take。
        return cstring.take(nativeText(mode),release)
    end
end
Reader reader=Reader()
func(...) as cstring read=reader.Text
string text=read(1,"two") // mode 的默认值在每次调用时计算
print(text)              // 保留 C 生产者指针，读取不复制

auto optional=reader.Text as (func(...) as cstring)?
if optional!=null
    string second=optional(1,"two")
    print(second)
end
```

`nativeText` 和 `release` 按具体库声明，释放器沿用 `void(ptr)` 合同。
`cbytes` 与 `***` 使用相同绑定机制。完整可运行程序见
`test/cases/extern_owned_bound_variadic_owners.xl`。

形参映射把包放在被调用方法的最后一个物理位置，省略的固定参数必须有默认值。
现有 typed 调用体直接加载并借用入参包，保持虚分派、默认值异常和接收者逃逸
生命周期。泛型类参数与方法参数仍使用完整、有序的具体化键。

## 成本和安全合同

- 精确 `cstring/cbytes` 返回保留原指针，使用已有的一个 C owner 记录。
- 目标签名明确要求普通返回 ABI 时，只在同一个调用体中复制对应结果叶子；
  积类型的其他 owner 字段直接移动。
- 绑定保留已有的闭包分配、接收者引用和代码生命周期管理成本。可空提升不增加
  第二个闭包；参数包不复制；没有新增转发包装层、动态类型标记或字符串读分支。
- IR 合同验证每个绑定仅捕获一个接收者、仅调用一次底层方法，包来自最后一个
  回调入参的直接 LOAD，复制数与请求的结果签名一致。错误包操作数、包擦除或
  选择证据必须在 C 和头文件输出前拒绝。

本轮不改变原生 C `c_vararg` 的独立 ABI，也不把任意擦除后的 var 声明为
compiler-only owner 签名。普通函数调用的必需实参数量规则保持原状；需要
省略源方法默认参数时，使用相应的较短精确函数签名。

这些验证证明本轮路径的指针、分配与生成结构合同，不是全部扩展库与手写 C
等性能的基准结论。`@owned string` 仍要求真正兼容 xrtMalloc/xrtFree 的分配器，
相同 CRT 或编译器不能替代该合同。

## 验证入口

耐久入口为 `tools/check_extern_owned_string.py` 和
`tools/check_extern_owned_string_public.py`。新增三组正常程序、五组编译反例和
三个原生 IR 合同，全部接入上述门禁。可空方法转换扫描 128 个分配失败前缀，
可变参数 owner 回调扫描 512 个前缀；每次检查 XRT 存活分配和外部分配器平衡，
并要求同时存在成功和失败样本。

另外重新执行原始审计的 32 个程序，其中两项错误程序必须编译拒绝，其余
30 项各执行 C 生成、JIT、AOT 构建和 AOT 运行，共 122 个步骤。
`nullable-owner-direct` 保留原审计文件，回归副本将一次 owner 记录的计数放到
回调清理前测量：临时闭包正常释放产生的 FreeCalls 不应被误计为字符串复制。
调用方继续核对原生产者指针。

最终验收回执见 `build/owned-string/callable-fixes-110/acceptance.json`。
本轮交付源码与测试，不替换主工作区发布二进制。

| 验证 | Windows | Linux / WSL |
| --- | --- | --- |
| 源码候选 | driver-110，1,378 编译单元，O0 | linux-110，1,380 编译单元，O0 |
| 公开接口链路 | 240 步通过 | 240 步通过 |
| 独立 C / IR 合同 | 47 步通过 | 47 步通过 |
| 原始审计回归 | 122 步通过 | 122 步通过 |

Windows 增量对象另经 GCC 实际依赖复核，45 个编译器单元中受影响的三个
均已重编，没有遗漏。Linux 候选为当前源码生成的新对象，补全其静态 TCC
生成头和 LZMA 链接依赖后完成链接；恢复构建保留失败日志及对象依赖核验，
不把失败尝试计为通过。Linux 在原生临时目录运行，9,952 个暂存源码和测试
文件与工作树及归档逐字节核对；验收结束后再次核对构建输入和对象哈希。
