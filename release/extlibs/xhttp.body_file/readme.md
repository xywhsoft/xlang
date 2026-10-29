# xhttp.body_file

`xhttp.body_file` 为 `xhttp.body` 提供惰性异步文件正文，不把文件整体读入内存，也不为每个对象预留固定缓冲。

- `open()` 在语言运行时共享的异步文件任务池中打开文件并取得稳定长度。
- `openRange()` 创建严格字节区间；区间越过文件结尾时 Future 失败。
- `adopt()` 接管一个可读 `AsyncFile`，保留直接组合底层文件能力的路径。
- `BodyFileConfig.readSize` 只是单次异步读取上限，默认 64 KiB；实际 Chunk 仍受调用方读取上限约束。
- 文件正文是单消费、不可重放正文。正文、Reader、Chunk 和 Future 都使用明确的拥有权与借用关系。

验证：

```text
python tools\sync_xhttp_body_file_package.py --xrt-root <xrt-root>
release\xl.exe run release\extlibs\xhttp.body_file\test\xhttp_body_file_test.xl
release\xl.exe build release\extlibs\xhttp.body_file\test\xhttp_body_file_test.xl -o out\xhttp_body_file_test.exe
```
