# unionbits - union 类型验证工具

定义 `IntFloat`（int64 ↔ float64）与 `ColorRGBA`（uint32 ↔ uint8）两个 union，外加 class 内匿名 union `Packet`，全面验证 xlang union 的 6 项能力，并探测嵌套 struct 边界。

## 覆盖的能力

| # | 能力 | 验证方式 |
|---|---|---|
| 1 | union 定义 + 字段访问 | `union IntFloat ... end`；`u.i = 42`、`u.f = 3.14`；最后写入者生效 |
| 2 | 字段位重解释 | 写已知 IEEE754 double 位常量读 float64（1.0/2.0/1.5/0.5/-1.0/-2.0）；反向写 f 读 i 位；`f→bits→f` 无损往返（pi 双精度）；uint32↔uint8 字节重叠（小端 LSB） |
| 3 | union C ABI 元数据 | `type(u)==IntFloat` 身份保持；`var` 装箱 + `as IntFloat` downcast 字段不丢；`export uIdent(IntFloat) as IntFloat` 触发 ABI 生成；外部 `xl abi` dump 确认 union 进入元数据 |
| 4 | union 作函数参数/返回值 | `func makeIntFloat(int64) as IntFloat`、`func readI(IntFloat) as int64`；跨函数位穿透 `roundTripF2I` / `roundTripI2F` |
| 5 | union 数组 | `array<IntFloat>` push / 索引读写 / 字段修改 / 拷贝隔离 / `var` 装箱类型身份 |
| 6 | class 内匿名 union | `class Packet { int tag; union { int code; c_double ratio } }`，旁路字段 + 匿名 union 字段读写 |

## 运行与验证

```bat
cd /d D:\GIT\x-lang\release
xl run  projects\dev\unionbits\unionbits.xl
xl build projects\dev\unionbits\unionbits.xl -o projects\dev\unionbits\unionbits.exe
```

两条路径均 EXIT=0，输出 51 条 `[PASS]`，`failed=0`。

补充验证 union C ABI 元数据生成（可选）：

```bat
xl abi projects\dev\unionbits\unionbits.xl
```

输出中可见 `export unionbits_ident` 的签名为 `fn:app.uIdent(IntFloat) as IntFloat`，多个函数签名（`makeIntFloat`、`readI`、`readF` 等）也正确引用 `IntFloat` union 类型。

## 设计要点

- **位重解释用已知 IEEE754 常数**：直接比对 `u.i = D1_0_BITS` 后 `u.f == 1.0`（`0x3FF0000000000000`），反向亦然。负数 `DNEG1_BITS = -4616189618054758400`（`0xBFF0000000000000`）。比浮点近似更严格——`expectFExact` 用 union 自身做位级比较，规避 NaN!=NaN 干扰。
- **union 语义**：所有字段从 offset 0 开始重叠。`ColorRGBA` 中 `uint32 rgba` 与 `uint8 lowByte` 在 offset 0 重叠；小端 x86 上 lowByte = rgba 的 LSB。写 lowByte 只覆盖最低字节，高 3 字节保留。
- **ABI 元数据**：通过 `export "unionbits_ident" func uIdent(IntFloat) as IntFloat` 强制编译器为 union 生成 C ABI wrapper（参数 + 返回均为 union 类型），从 xlang 侧调用该 export 验证 wrapper 可用；外部 `xl abi` 二次确认。
- **main 始终返回 0**（发现 BUG 只记录不中断），保证 `xl run`/`xl build` 双路径 EXIT=0。

## 发现的 BUG / 限制（详见 dev/BUGS.md）

- **BUG-U1（低）**：`string(type(u))` 对 union 实例返回字面串 `"unknown"`，丢失类型名。**类型身份比较本身正确**（`type(u) == IntFloat`、`var` 装箱后 `type(boxed) == IntFloat`、`is IntFloat` 全部为 `true`），仅 `string(type_value)` 这一路丢名。对照 class 实例 `string(type(obj))` 可正常返回类名。绕过：用 `type(u) == T` 身份比较或 `u is T`，不依赖字符串化取名。
- **限制（非 BUG）**：union 字段不支持匿名嵌套 struct。`union Color { uint32 rgba; struct { uint8 r,g,b,a } }`（多行 `struct ... end` 或单行 `struct { ... }`）均报 `[parse] 语法错误`。DEMO6 功能清单第 11 节未声明支持嵌套 struct 字段，属设计边界。需字节拆解时用 `uint32` + 单 `uint8` 别名（如本工具 ColorRGBA），或 `int64` 承载 + 位运算自行拆。
