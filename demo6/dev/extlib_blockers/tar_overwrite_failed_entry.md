# TAR 覆盖提取在单条目失败后改写旧文件

## 已验证行为

`tar_overwrite_failed_entry.c` 先在独占临时目录写入 `one.txt=OLD`，再构造
`one.txt=NEW` 的 TAR，只向 reader 提供完整 512 字节头和 2 字节正文。
调用 `xrtTarReaderExtractAll(reader, output, true)` 后，该条目读取失败，
但目标已不再是 `OLD`。Windows 严格 GCC 与 Linux GCC ASan/UBSan 均输出：

```text
reproduced: failed entry changed existing OLD file
```

这是**单个文件写入未成功**时的旧数据破坏，不是“整份归档取消后已完成条目仍保留”
的事务语义争议。直接原因位于 `xrt_tar_extract.c` 的
`__xrtTarExtractFile`：覆盖模式通过锚定根句柄打开目标时带
`XFILE_TRUNCATE`，之后才将 TAR 正文写入；读、写、刷新或关闭失败
无法恢复旧内容。

## 复现命令

从 `demo6` 目录使用包级 `test/runtime.c` 和 `xrt_tar_{common,reader,writer,extract}.c`
编译 `dev/extlib_blockers/tar_overwrite_failed_entry.c`，例如 Windows：

```powershell
gcc -std=c11 -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/tar release/extlibs/tar/xrt_tar_common.c release/extlibs/tar/xrt_tar_reader.c release/extlibs/tar/xrt_tar_writer.c release/extlibs/tar/xrt_tar_extract.c release/extlibs/tar/test/runtime.c dev/extlib_blockers/tar_overwrite_failed_entry.c -o <temporary-exe> -lws2_32 -lbcrypt -lshlwapi
<temporary-exe>
```

## 修复边界

保持 `overwrite=true` 的公开能力，不用提前拒绝覆盖或读完整归档到内存来
伪造通过。应先把单条目写入同一父目录下、由同一 `xroot` 锚定的独占临时文件；
只有完整读取、刷新和关闭成功后，才以根内、末级不跟随链接的原子改名提交，
失败时清理本次临时文件并保留旧目标。`overwrite=false` 还需保证提交时
目标仍不存在。后续再单独定义目录、链接条目和整份归档的事务语义；
**逐文件原子提交不等于整份归档全有或全无**。

独立 XRT 仓库已有根内改名 API，但 DEMO6 的 `lib/xrt/xrt.h` 当前没有
`xrtRootFileRename`。须由 XRT 单头文件同步及相应根内临时文件能力先到位，
再改 TAR 包并增加“损坏正文/写入失败/并发替换”的红绿回归。
本任务不修改 xlang 主线，也不在 TAR 复制 Windows/POSIX 文件实现。
