# sharedmem 双语范例

从仓库根目录运行，Windows 使用 `release/xl.exe`，Linux 使用 `./release/xl`。
名称每次随机生成，不需要管理员权限、网络服务或外部文件。
两个进程同时读写仍须自行约定同步，本例没有演示无锁并发协议。

| 范例 | XLang | C | 预期 |
| --- | --- | --- | --- |
| 基础读写 | `release/examples/extlibs/sharedmem.xl` | `sharedmem_c.c` | `basic.expected.txt` |
| 快照与生命周期 | `snapshots.xl` | `snapshots_c.c` | `snapshots.expected.txt` |

```powershell
release/xl.exe run release/extlibs/sharedmem/examples/snapshots.xl
release/xl.exe run release/extlibs/sharedmem/examples/snapshots_c.c
release/xl.exe build release/extlibs/sharedmem/examples/snapshots.xl -o build/sharedmem-snapshots.exe
./build/sharedmem-snapshots.exe
```

新快照例的 C/GCC、C 内存运行、C 独立产物、XLang 内存运行和独立产物
在两平台均逐字对照以下输出：

```text
peer survives owner: true
little endian: 0807060504030201
new data zero: true
snapshot text bytes: 8
snapshot binary: 4100e4bda0e5a5bd
```

整数从偏移 1 开始，不要求对齐，固定小端；文本在偏移 16，占 8 字节，
包括 `A`、一个 NUL 和 `你好`。创建者退役后已打开的 peer 仍可读写。
`zero()` 只清映射内容，不修改已读取的独立快照。返回 `(cstring, Buffer)`
后所有映射句柄已退役，文本和二进制仍有效。

C 显式调用 `xshm_close`、`xrtFree` 和 `xrtBufferDestroy`；XLang 按对象
作用域及返回值所有权退役。不将句柄地址序列化进共享段，不使用裸指针
跨进程。POSIX 创建者关闭会移除名称，已打开映射保留；Windows 则等到
最后句柄关闭，因此不要依赖创建者退役后的同名 `open()` 跨平台一致。

文本读取必需复制一次并验证 UTF-8，之后 `cstring.take` 接管，不再经
Buffer 转字符串二次复制；二进制读取也返回拥有型副本，而非映射借用。
这不是整个语言调用路径零分配或零成本的承诺。

不通过 `xl` 的独立 GCC 对照（Windows；GCC 已在 PATH）：

```powershell
gcc -std=c11 -O2 -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/sharedmem release/extlibs/sharedmem/examples/snapshots_c.c release/extlibs/sharedmem/sharedmem_native.c release/extlibs/sharedmem/test/xrt_runtime.c -lws2_32 -ladvapi32 -lm -o build/sharedmem-snapshots-c.exe
./build/sharedmem-snapshots-c.exe
```

Linux 同样编译三个源文件，移除 Windows 链接库并加 `-D_GNU_SOURCE -pthread
-lm -ldl -lrt`。完整 sanitizer/跨进程/原型/故障验证由 `test/check.sh` 执行。
测试运行库只启用本包所需模块，不再使用 `XRT_MODULE_ALL`，不改变生产 XRT。
本次没有测量吞吐或进程间可见性延迟。

基础 XLang 范例和包内部分顶层资源 AOT 仍受 M2 退出问题阻断；私有构造器
负例仍受 M3 阻断。完整门禁会明确失败并保留本次现场，不因新范例通过而
跳过这些必过项；`-SkipAot` 只供定位，不能代替发布验收。
