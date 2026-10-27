# 历史记录：旧 Linux AOT 的普通输出缺少字典图提供者

2026-10-27 从已提交源码 `f715e057e` 在独立工作树重建 Linux 发布版后，
本页无包最小源的 JIT、独立 EXE 构建与运行全部输出 `ok`、退出 0；
`release/extlibs/dotenv/test/check.sh` 的 C ASan/UBSan、XLang JIT/AOT
与双语输出对照也全部通过。先前缺符号只对应 2026-10-26 的旧
Linux 发布物，不再列为当前阻断。包门禁另发现 Windows 工作树将
文本黄金文件检出为 CRLF；脚本现只规范化黄金文件行尾后比较。

## 旧发布物复现

使用 `dev/extlib_blockers/linux_aot_print_graph_provider.xl`，不导入任何扩展库：

```sh
cd demo6
release/xl run dev/extlib_blockers/linux_aot_print_graph_provider.xl
release/xl build dev/extlib_blockers/linux_aot_print_graph_provider.xl -o build/test/print_probe
```

2026-10-27 本机现有 Linux `release/xl`：`run` 输出 `ok`、退出 0；
`build` 报 `tcc: error: unresolved reference to 'xl6DictGraphProviderV1'`、退出 1。
相同源在当前 Windows `release/xl.exe` 构建为独立 EXE 后输出 `ok`、退出 0。
Linux 发布物时间为 2026-10-26；后来已按文首所述用新构建复验通过。

`release/libxlc.so` 的动态符号表含 `xl6DictGraphProviderV1`，说明符号
存在于编译器库；生成的主模块 C 仅调用 `xl6_xrt_print`，没有显式引用
图提供者。`src/runtime/xl6_print_values.c` 的开放式打印实现直接引用
List/Set/Dict 三个图提供者。当前证据指向 Linux 独立产物的运行库依赖
闭包或链接输入，而非 dotenv 的解析器；具体遗漏环节仍需主线定位。

## 当时受影响的验收

`release/extlibs/dotenv/test/check.sh` 的 C ASan/UBSan 与语言 JIT 已通过，
中央 `extlib_dotenv.xl` 独立 EXE 也可构建运行；随后构建带 `println` 的
`release/extlibs/dotenv/test/limits.xl` 时遇到同一缺符号，整包 Linux
门禁退出 1。新发布版的完整门禁已通过，不能继续用旧失败描述当前包。

## 回归边界

构建 Linux 发布物后应先运行上述无包最小源的 JIT/AOT，再运行
dotenv 默认门禁。该次复验表明现有已提交源码满足这条路径；不能
用旧 `release/xl` 的失败推定当前源码缺少传递依赖。
