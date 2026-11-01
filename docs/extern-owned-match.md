# C 字符串的 match 与回调签名

`match` 表达式沿用静态 `cstring/cbytes` 的读取、物化和释放规则。它不增加运行时类型标记、普通字符串读取分支或额外的回调转发层。

## 方法组接收期望签名

和条件表达式一样，`match` 的每个方法分支都接收目标函数签名，不必逐个分支写 `as`：

```xlang
typedef Text=func() as cstring
func select(bool choose,Reader left,Reader right) as Text
    return match choose
    case true then left.Text
    case false then right.Text
    end
end
```

`Reader.Text` 返回 `cstring` 时，调用选中的回调保留 C 生产者指针。`cbytes`、泛型方法、枚举载荷接收者、私有方法的合法作用域，以及省略默认实参的原生方法，使用相同规则。候选重载探测不发布分支转换；最终选定签名后才绑定。模式绑定复用原有符号身份，允许遮蔽同名外层变量。

`@owned` 原生方法已经使用普通 `string` 返回 ABI，绑定为 `func() as string` 时继续接管原缓冲区。它要求真正兼容 `xrtMalloc/xrtFree` 的分配器；仅使用同一 CRT 或同一编译器不构成兼容性保证。

## 值分支与顺序

局部字符串的静态载体由全部分支决定：普通 `string` 与 `cstring`、普通 `bytes` 与 `cbytes` 混合时，都保留 owner 载体，与分支书写顺序无关。C 分支读取不复制，普通分支使用已有的载体转换。`null` 与已有可空值合并保持单层可空类型。

```xlang
string text=match choose
case true then "ordinary"
case false then cstring.take(nativeText(),nativeFree)
end
print(text)       // C 分支保持原指针
text[0]='X'       // 沿用现有写入物化，其他引用仍然有效
```

## 重载与明确的复制边界

精确 owner 回调签名优先于需要复制结果的普通回调签名。省略默认实参数量仅在转换成本相同后参与排序；可空提升也保留其转换成本。此规则适用于直接方法组、条件/合并表达式及已存储的函数值，包括类型别名。

显式要求 `func() as string` 接收一个返回 `cstring` 的语言方法时，结果依旧按普通返回 ABI 复制；使用 `func() as cstring` 保留原指针。这和原生 `@owned string` 的接管路径是不同的所有权合同。类型别名不会改变转换规则。

C owner 仍有已有的 owner 记录，方法绑定仍有闭包、接收者引用及代码生命周期成本；C 字符串接管仍须确定长度。本功能不承诺整个 OOP 调用与手写 C 等速。

混合 `bytes/cbytes` 的普通 `bytes` 分支需要一个现有的 C owner 记录，转换本身不复制缓冲区。实测单独的 bytes 字面量为一次 XRT 分配，装入混合 match/条件表达式后为两次；本轮使两种 match 顺序与既有条件表达式一致。普通 string 装入该载体无需这个记录，单独的 bytes 路径也不增加成本。消除这个混合二进制路径的记录分配需要另外调整载体或生命周期设计，未在本轮引入。

## 回归入口

运行 `tools/check_extern_owned_string.py`，并用 `--runtime-build` 提供对应源码构建回执，即可同时执行独立 C 头文件消费及 IR 合同门禁。新增程序：

- `test/cases/extern_owned_bound_match.xl`：接收者只求值一次、模式变量遮蔽、逃逸回调、默认参数及 512 个分配失败前缀。
- `test/cases/extern_owned_match_values.xl`：两种分支顺序、字符串/二进制、可空类型、写入与引用，以及 384 个分配失败前缀。
- `test/cases/extern_owned_bound_conversion_rank.xl`：复制/可空提升成本、类型别名、泛型、多返回值及 512 个分配失败前缀。

编译反例继续验证私有成员和泛型约束；原生合同检查精确调用体、接收者捕获与读取路径复制数，并验证损坏的选择证据在生成 C/头文件前被拒绝。
