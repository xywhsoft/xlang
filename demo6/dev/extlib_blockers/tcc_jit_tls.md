# TCC JIT 的 `__thread` 变量未形成可靠线程隔离

状态：主线待排查。本任务不修改 TCC、xlang 编译器或运行时。

## 最小复现

两份源文件：`tcc_jit_tls.c` 和 `tcc_jit_tls_helper.c`。前者在两个存活线程中
取得辅助编译单元里的 `static __thread int` 地址，并比较指针。

从 `demo6` 目录运行：

```powershell
gcc -std=c11 -Wall -Wextra -Werror dev/extlib_blockers/tcc_jit_tls.c dev/extlib_blockers/tcc_jit_tls_helper.c -o build/test/op/tcc_jit_tls_gcc.exe
./build/test/op/tcc_jit_tls_gcc.exe
./release/xl.exe run dev/extlib_blockers/tcc_jit_tls.c dev/extlib_blockers/tcc_jit_tls_helper.c
```

本机 GCC：`same TLS address: 0`，退出码 0。`xl run`：退出码 1，
没有输出诊断。单看这条命令，不能区分编译、重定位和运行时入口问题。

另在 `op_native.c` 暂时把 64 KiB 静态结果缓冲改为 `static __thread`，
TCC 对象检查显示 `.tbss` 为 64 KiB，但通过 `xl run` 调用
`release/extlibs/op/test/text_threads.c` 时，两个线程得到同一指针，
且第二个线程的结果覆盖第一个线程的文本。GCC 严格语法检查通过。
`__declspec(thread)` 虽能被 TCC 编译，却仍生成普通 `.bss`，不能使用。

## 影响与建议

不能凭对象文件出现 `.tbss` 就认定 JIT TLS 可用。主线应分别检查 TCC
生成的 TLS 访问、`tcc_relocate` 对 TLS 节区/重定位的处理，以及 C-only
`xl run` 的错误传播；修复后以双线程运行用例验证地址和值隔离。

`op` 已撤回 TLS 尝试，仍使用原有共享缓冲，尚不具备并发文本结果安全性。
包级修复方向是返回独立拥有的字符串并在 XLang/C 调用边界明确释放，
不以本主线修复作为前置条件。
