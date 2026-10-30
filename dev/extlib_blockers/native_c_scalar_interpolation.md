> 历史工作树记录：以下结果对应文内冻结版本，不自动代表主目录当前 0.6.0；需在当前发布物上复验。

# C 标量类型的 f-string 转换缺失

状态：blocked-upstream（M11），当前直接受影响包为 cString。普通扩展库
任务不修改主线，不把 `length` 的 `c_size` 返回类型改成有符号 `int`，也不在
范例中强转、借助 var 或改用其他输出语法来掩盖该边界。

## 无包最小复现

```xlang
func main(...) as int
    c_size count = c_size(6)
    println(f"bytes={count}")
    return 0
end
```

保存源为 [native_c_scalar_interpolation.xl](native_c_scalar_interpolation.xl)。
在仓库根目录 下执行：

```powershell
./release/xl.exe check dev/extlib_blockers/native_c_scalar_interpolation.xl
./release/xl.exe run dev/extlib_blockers/native_c_scalar_interpolation.xl
./release/xl.exe build dev/extlib_blockers/native_c_scalar_interpolation.xl -o build/test/c_scalar.exe
```

Windows 的 `check` 返回 0，`run` 和 `build` 返回 1，诊断：

```text
[checked] XLIR interpolation expression has no declared string conversion ABI
```

真实消费正例为 [cString 的 records.xl](../../release/extlibs/cString/examples/records.xl)：
`cString.length(text)` 返回 `c_size`，放入 f-string 时同样失败。其 C 对照
可正常生成六行黄金输出；语言例保留原写法并进入默认门禁，当前未签收可运行。

## 已确认范围与证据

固定基线为本工作树的 `d64672903`，不是持续更新的 master。发布物哈希见
[验收矩阵](../../docs/普通扩展库发布验收矩阵_2026-10-28.md)，主线源与 XRT 未改。

Windows/Linux 均检查以下 24 种类型各两种写法，共 48 项：

- 普通 `int/uint64/float/bool` 的直接打印和插值均返回正确结果。
- 以下 20 种 C 标量类型的直接打印返回正确结果，插值均失败：
  `c_char/c_schar/c_uchar/c_short/c_ushort/c_int/c_uint/c_long/c_ulong/`
  `c_longlong/c_ulonglong/c_size/c_ssize/c_ptrdiff/c_intptr/c_uintptr/`
  `c_bool/c_float/c_double/c_wchar`。
- `c_bool` 在语义阶段报告“f-string 插值表达式不能转换为 string”；
  其余 19 种在 checked lowering 阶段报告上面的缺少 ABI。
- 对照值为整数 6、浮点 6.5、布尔 true；直接输出和普通类型的插值正文
  均逐项核对，不把退出码为零误作正确输出。这不是极值或全格式认证。

Windows 48 项源、原始 stdout/stderr 与输入哈希在
`build/test/c_scalar_interpolation_063e2bf5c4204563ae459949ec3f98aa/results.json`。
Linux 同一批源复制到原生文件系统快照
`/var/tmp/xlang-cstring-interop-src-5EicMQ/probes/`，逐文件保存退出码与输出，
`probes-results.tsv` 与 `probes-summary.txt` 记录 20 项同样拒绝、意外差异 0。

cString 的默认 Windows 门禁共 25 阶段，前 24 个正向阶段通过，最后的
records 语言例返回 1；报告
`build/test/cString_guard/run_aea650dba47f408c8f3239613bf8b632.json`，输入变动 0。
包括 528 个真实 CRT 缓冲、实际 `strlen` 原型、内嵌 NUL/UTF-8/切片、
全局/局部/传参/返回函数值、旧双语输出和五条独立程序路径。Linux 同一
快照的 C ASan/UBSan、这些 JIT/AOT 和 C 黄金输出也先通过，最后在插值
失败；内外输入核对一致，父子现场和真实退出 1 保留。

不从类型矩阵推断其他扩展包已失败，当前 M11 涉及包数只计 cString。

## 主线定位与修复建议

以下行号属于上述固定源码基线：

- `src/ir/xl6_ir.c:11142` 的 `xl6IrClassifyStringConversionAbi` 按 `cAbi`
  分类，覆盖普通 I64/U64/F32/F64/BOOL 等，但没有原生 C 标量族。
- `src/ir/xl6_ir_lower.c:9690` 调用该分类器，INVALID 时在 9693 发出
  checked 诊断。不能只删除诊断，否则后续没有正确的转换 ABI。
- `src/sem/xl6_sem.c:16658` 先检查隐式字符串转换；`c_bool` 比其余 C
  标量更早被拒绝，须让语义检查与后续 lowering 采用一致的能力分类。

建议在共享类型元数据/转换分类中将 C 标量按目标平台真实宽度、符号性和
布尔/浮点类别解析到既有格式化能力，或补显式的原生标量转换 ABI。不要
用 `int64` 统一接收无符号值，也不要依靠类型名字在各扩展包内写适配器。
`c_char/c_wchar` 的显示应与当前直接打印的数值合同一致，不能悄悄改为字符。

主线修复后的验收应包括：

1. 20 种类型的字面量、变量、C 返回值、字段、条件表达式、泛型/函数值
   返回插值；普通类型对照与 JIT/AOT 同时通过。
2. 有符号最小/最大值、无符号上半区及最大值、负零/NaN/无穷等既有
   浮点格式合同；Windows LLP64 和 Linux LP64 下 `c_long` 等宽度差异。
3. 不改变 `var/xcall` 对完整无符号全值域的准入规则，不放宽无关的
   类型转换或受检查提取。
4. 复跑无包最小源、cString 默认双平台门禁及 records 双语黄金输出，
   不以 `-SkipAot` 或强制转换替代真实正例。
