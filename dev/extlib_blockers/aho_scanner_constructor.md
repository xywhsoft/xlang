# Aho Scanner 原生构造器访问边界

`Scanner` 是导出的 native-backed class，`Automaton.scanner()` 是预期的安全创建入口。但类仍声明 `extern "xac_stream_create" create(point automaton)`，包外可直接传入任意 `point`。最小复现：

```powershell
release/xl.exe check dev/extlib_blockers/aho_scanner_constructor.xl
```

当前返回 `ok`。此探针只编译，不运行；不要执行伪造句柄调用。`Scanner` 内部还有所有权和分块状态约束，因此公开原生 `point` 构造不应作为正常 API。

不能仅将构造器改为 `private`：当前语言的 `private` 是类级作用域，`Automaton.scanner()` 的跨类构造也会被拒绝；其它 native-backed class 的私有原生构造器还存在包外检查未阻止调用的主线问题。主线应先明确并实现可信的包内工厂/原生句柄接管权限，同时保证包外无法调用私有构造器。修复后将 `Scanner` 构造器收紧，并分别检查包外负例、`Automaton.scanner()` 正例及所有权/析构回归。本任务不修改编译器。
