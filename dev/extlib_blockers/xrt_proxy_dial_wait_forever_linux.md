# 当前 XRT 全模块编译失败：代理拨号超时常量未定义

2026-10-21，在当前共享 `demo6/lib/xrt/xrt.h` 工作树上，Linux 编译
`XRT_MODULE_ALL` / `XRT_IMPLEMENTATION` 时，代理拨号实现使用了
未声明的 `XRT_WAIT_FOREVER`。与 `srt` 包实现无关。该头文件属于
独立维护的 XRT/主线，本扩展库任务不修改它。

最小复现，在 `demo6` 目录执行：

```sh
printf '#define XRT_MODULE_ALL\n#define XRT_IMPLEMENTATION\n#include "xrt.h"\n' |
  gcc -std=c11 -D_GNU_SOURCE -Ilib/xrt -x c -fsyntax-only -
```

当前错误：`xrt.h:258021:78: error: 'XRT_WAIT_FOREVER' undeclared`。
建议由 XRT 维护侧核对 `xnetproxydialconfig.Timeout` 的哨兵合同：
声明统一的无限等待常量，或删除无效的比较并使用已有的零值/超时规则；
修复后以全模块编译和代理拨号超时用例一并验证。

`srt` 的独立测试宿主已改为仅编入本包 manifest 声明的 Buffer、
Value Container、Unicode 与测试所需的 Memory Debug 模块；
这不会修复或隐藏全模块模式下的上述 XRT 编译失败。
