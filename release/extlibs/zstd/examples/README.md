# Zstd 双语范例

从仓库根目录运行，无外部素材，不写数据文件：

```powershell
./release/xl.exe run release/extlibs/zstd/examples/basic.xl
./release/xl.exe run release/extlibs/zstd/examples/basic_c.c
./release/xl.exe run release/extlibs/zstd/examples/bounded_stream.xl
./release/xl.exe run release/extlibs/zstd/examples/bounded_stream_c.c
./release/xl.exe build release/extlibs/zstd/examples/bounded_stream.xl -o bounded_stream.exe
./bounded_stream.exe
```

Linux 将 `./release/xl.exe` 改为 `./release/xl`，独立程序不加 `.exe`。
C 源同样可以 `build`；两种语言的 basic 都输出
`zstd round trip: hello zstd`。bounded_stream 都自行断言并输出：

```text
source bytes: 7
completed frames: 1
binary round trip: true
output limit rejected: true
zstd bounded stream: OK
```

输入分为 `one\0` 和 `two`，NUL 是真实的二进制字节。每次 `drain`
只取当前产出，可能为空；`finish` 写出帧尾，之后还需最后一次 drain。
解压器的 `finish` 校验帧完整性；示例同时检查原文和完整帧数。
输出预算1字节会产生范围错误，随后用新操作正常往返，不能把已失败
的流重新当作有效流继续使用。

C 创建的流分别 Destroy，drain 和一次性结果分别 BufferDestroy；
XLang 自动释放。输入仅借用，返回 Buffer 独立拥有。正常分段例不
注入内存故障；后端、XRT 分配故障与失败恢复专项放在 `test`。
本例在函数内演示资源使用，并不替代仍失败的全局对象 AOT 测试。
