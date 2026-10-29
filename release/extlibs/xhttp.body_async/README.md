# xhttp.body_async

`xhttp.body_async` 在 `xhttp.body` 的单消费 Reader 状态机上增加可读性 Future，不把 Future、等待和同步原语强制带入同步正文包。

- `AsyncBodyReader.next()` 保留 `DATA/EOF/AGAIN/ERROR` 底层状态。
- `waitReadable()` 只允许在 `AGAIN` 后调用，返回的 Future 与 Reader 生命周期独立。
- `nextAsync()` 在 `AGAIN` 时等待并重试，常见异步读取不需要手写状态循环。
- `nativeHandle()` 保留直接调用 XRT C API 的能力。

同步与测试：

```text
python tools\sync_xhttp_body_async_package.py --sync --xrt-root <xrt-root>
release\xl.exe run release\extlibs\xhttp.body_async\test\xhttp_body_async_test.xl
release\xl.exe build release\extlibs\xhttp.body_async\test\xhttp_body_async_test.xl -o out\xhttp_body_async_test.exe
```
