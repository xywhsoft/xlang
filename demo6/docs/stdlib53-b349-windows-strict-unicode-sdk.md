# Windows 内嵌 SDK 的严格 Unicode 转换

Windows 内嵌 TCC 的 Win32 头缺少 `WC_ERR_INVALID_CHARS`，导致实际产品
在编译携带 xlang 宿主的字符串桥接程序时失败。系统 GCC 使用另一套头，
所以普通 DLL 构建未暴露这个缺口。本次仅补齐这个常量，不修改 Unicode
转换实现、编译器的 C 转换规则或目标 Windows 版本。

## SDK 合同

`WC_ERR_INVALID_CHARS` 的值是 `0x00000080`。对于 UTF-8 转换，它要求
`WideCharToMultiByte` 拒绝非法 UTF-16，不能用 `0` 代替；宽松转换会把
非法代理项替换为 U+FFFD。依据为
[微软 SDK 定义](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/um/WinNls.h)及
[WideCharToMultiByte 合同](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte)。

## 实际产品验证

`check_stdlib_windows_nls_sdk_shipping.py` 的消费者直接包含 `<windows.h>`，
没有本地常量兜底，也没有替代转换实现。系统 GCC 只构建公共 API 宿主；
实际 runtime／compiler DLL 的内嵌 TCC 和 VFS 编译、运行消费者。

两种 DLL 各有 O0／O2 两个宿主配置，共四个配置通过 JIT 和模块重定位。
这里的优化等级指 GCC 编译的宿主，不是声称 TCC 消费者接受了 O0／O2 优化。
消费者验证显式长度保留内嵌 NUL、BMP 与非 BMP 编码、输出容量及哨兵、
完整代理对和截断代理对、独立高／低代理项，以及宽松转换的替代字符对照。
编译器 DLL 另生成两个可搬移的程序，检查没有 xl／xlc DLL 依赖后独立运行。

缺失常量、错误值和宽松转换分别在两种 DLL 上验证，共六个反例；只接受
明确退出码与预期诊断，不把崩溃、超时或任意失败当作反例通过。旧产品的
四个配置均复现缺失常量，旧产品拒绝报告不计作新产品运行通过。

```text
python tools/check_stdlib_windows_nls_sdk_shipping.py --root <frozen-demo6> --build-report <ordinary-build.json> --out <fresh-directory>
```

新产品构建凭据为 `D:/xlang-validation/stdlib53-b349-build-win/build.shipping.json`；
新验证为 `D:/xlang-validation/stdlib53-b349-sdk-new/results.json`；旧产品对照为
`D:/xlang-validation/stdlib53-b349-sdk-old-r2/results.json`。
Windows 产品聚合验收已将该测试登记为必需子项，Linux 不把它计作平台通过。

## 完整工作区的剩余边界

此前整合候选的两端各有 328 项语言清单，可执行的 JIT／模块用例全部通过；
产品专项分别为 Windows 24／25、Linux 26／27 通过，剩余项均是嵌套编译。
两端独立 AOT 清单各有一项 `stdlib_string_bridge_boundaries` 构建失败：
Windows 首先暴露本次 SDK 缺口，Linux 已报缺少 IDE 编译单元的引用。
这些失败报告保留，不把先前通过的专项挪算到新产品。

用户已允许独立整理五个 IDE 编译单元及必要前置，相关依赖闭包、SDK 的其他
所需声明和完整新产品批量验收继续进行，不纳入本次常量修复提交。
Windows DST 仍按用户决定独立待验收，不修改宿主时区或计作通过。
CRT 无未决策项；XRT 其余 19 组及对象池最后的顺序不变。
