# Linux AOT 的普通输出缺少字典图提供者

## 当前发布物复现

使用 `dev/extlib_blockers/linux_aot_print_graph_provider.xl`，不导入任何扩展库：

```sh
cd demo6
release/xl run dev/extlib_blockers/linux_aot_print_graph_provider.xl
release/xl build dev/extlib_blockers/linux_aot_print_graph_provider.xl -o build/test/print_probe
```

2026-10-27 本机现有 Linux `release/xl`：`run` 输出 `ok`、退出 0；
`build` 报 `tcc: error: unresolved reference to 'xl6DictGraphProviderV1'`、退出 1。
相同源在当前 Windows `release/xl.exe` 构建为独立 EXE 后输出 `ok`、退出 0。
Linux 发布物时间为 2026-10-26；本次没有覆盖重建它，因此须在主线修复前
先用新构建复验，不把旧二进制结果推断为当前源码的最终状态。

`release/libxlc.so` 的动态符号表含 `xl6DictGraphProviderV1`，说明符号
存在于编译器库；生成的主模块 C 仅调用 `xl6_xrt_print`，没有显式引用
图提供者。`src/runtime/xl6_print_values.c` 的开放式打印实现直接引用
List/Set/Dict 三个图提供者。当前证据指向 Linux 独立产物的运行库依赖
闭包或链接输入，而非 dotenv 的解析器；具体遗漏环节仍需主线定位。

## 受影响验收

`release/extlibs/dotenv/test/check.sh` 的 C ASan/UBSan 与语言 JIT 已通过，
中央 `extlib_dotenv.xl` 独立 EXE 也可构建运行；随后构建带 `println` 的
`release/extlibs/dotenv/test/limits.xl` 时遇到同一缺符号，整包 Linux
门禁退出 1。不能用中央用例成功替代完整门禁。

## 主线修复边界

统一审核 Linux AOT 对 `xl6_xrt_print` 实现的传递符号依赖：如果选择
开放式打印实现，就应按实际 C 依赖带入三个图提供者及其实现，而不是
由包或生成代码扫描文本补符号。新构建 Linux 发布物后，先运行上述
无包最小源的 JIT/AOT，再运行 dotenv 默认门禁及其他带输出的包级 AOT。
