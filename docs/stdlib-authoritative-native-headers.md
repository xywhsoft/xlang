# 标准库的权威原生头与 C++ 重载校验

2026-11-10，XGE 全量接入期间发现并修复的通用互操作问题。

## CRT 声明合同

正式发布编译器在同一程序使用 XGE 与 `math.abs` 时，第二个原生 C 编译单元会报
`fabs defined twice`。`sin`、`sqrt` 的对照可通过。原因是生成的公共头重新声明了
`extern double fabs(double)`，干扰 Windows TinyCC 数学头中的静态内联定义。

标准库合同表现在可以声明权威 `header`。CRT 实数数学入口统一标注 `math.h`，
由标准库注入普通 `@header` 声明，再经现有语义和封存 IR 传递。Cgen 包含原生头，
检查函数物理签名，不再自行重声明这些函数。未设置头的其他合同保留原有行为；
已有 pure/no-raise/nonblocking 效果保持不变。没有增加数学转发函数。

同时修复 Windows TCC SDK 的 `fabs/fabsf/fabsl`：原来的 `x < 0 ? -x : x` 不能清除
负零和带负号 NaN 的符号。现在清除符号位，保留 NaN payload；该 SDK 的 long double
采用原有 8 字节 ABI。没有修改系统 CRT 或其他平台的数学实现。

## C++ 公开头

C++ 数学头提供同名重载，直接 `decltype(&fabs)` 无法确定函数。新增单独的
`XLIR_NATIVE_IS_FUNCTION` 编译期谓词：普通函数继续按规范化签名严格比较；
重载集合由预期函数指针类型选择。它不改变记录字段、指针或数组的比较规则。
原有枚举底层整数 ABI 与 noexcept 规则继续保留。C 模式仍使用原有 `_Generic`
精确比较，不引入运行时调用。

正反例覆盖 C11、C++11/17、重载、枚举参数/结果、noexcept、错误参数和返回类型。
9 组既有互操作夹具的 C++11/17 头通过；投影重载的 20 项检查与 10 项封存 IR
篡改拒绝也通过。旧断言工具仅更新函数谓词名称，字段类型检查没有放宽。

## 构建与验收边界

`compiler-full-r4` 通过普通 `build.bat` 完整 O2 构建，重新生成 TCC、XRT、两份 VFS、
xl.dll、xlc.dll 与 CLI，输入未变。其上通过 12 项数学/原生 C/XGE CPU 检查，以及
25 项资源/Input Actions JIT、静态 AOT 与独立导入检查。

C++ 函数谓词是上述完整构建之后补入的 Cgen 修复，使用另一个诊断候选通过
`tools/check_stdlib_native_headers.py` 的 26 项检查及上述回归。它尚未进入新的完整
嵌套编译器/VFS 配套构建；不能把这份诊断候选当成正式交付。下一次整体构建仍需
纳入这两份 Cgen 源文件并复测嵌套编译。这些候选均未安装到正式 release/IDE。

完整证据留在 `F:/xlang-validation/xge-full-20261109/`，小索引在
`dev/release-materials/provenance/stdlib-native-headers-20261110.json`。本批不代表
XGE/XUI/Document 的其余功能或图形/音频/设备后端已经完成。
