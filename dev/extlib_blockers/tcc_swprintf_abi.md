# 内置 TCC 的 `swprintf` 声明与 C99 ABI 不一致

## 复现

最小代码：`dev/extlib_blockers/tcc_swprintf_abi.c`。

从 `demo6` 目录执行：

```powershell
gcc -std=c11 -Wall -Wextra -Werror dev/extlib_blockers/tcc_swprintf_abi.c -o build/test/op/tcc_swprintf_native.exe
./build/test/op/tcc_swprintf_native.exe
./release/xl.exe run dev/extlib_blockers/tcc_swprintf_abi.c
```

2026-10-13 本机 MinGW GCC 原生程序退出 0；`xl run` 退出 1、无诊断。
GDB 显示线程在 `msvcrt.dll!swprintf`（上层 `ungetwc`）访问违例。

## 根因与影响

`res/tcc/win64/x86_64/include/stdio.h` 当前声明
`swprintf(wchar_t *, const wchar_t *, ...)`，并把 `swprintf` 链到
`msvcrt.dll` 的旧式两参数入口。C99/C11 的常见写法为
`swprintf(buffer, capacity, format, ...)`；内置 TCC 没有拒绝这个调用，
第二参数 `capacity` 被 msvcrt 当成格式串指针使用，导致进程崩溃。
这也影响需要在 GCC 与 `xl run *.c` 之间共用源码的扩展库 C 测试。

## 处理建议

主线应明确 Windows C 运行库选择及标准函数口径：提供正确的三参数
`swprintf` 实现/重定向，或在 TCC 编译阶段对不兼容签名给出错误；
不应继续让 C99 写法无诊断进入旧 ABI。修复后以本最小样例做双编译器
回归，并检查 `vswprintf` 同族声明。此任务不改 TCC/编译器主线。

`op` 包门禁只拼接固定短事件名，已使用 Win32 `wsprintfW` 且证明输出
小于目标数组；它不是通用格式化的兼容层。
