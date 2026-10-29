# xml.sax 原生字符串实参与内部 helper 的符号可见性

## 当前可用路径

`release/extlibs/xml.sax/src/sax.xl` 以 `point` 调用 `xsax_open_memory`，
先将 `string` 写入 `Buffer`；当前公开 C/XLang 范例和边界用例均可运行。
本包已在创建该临时缓冲前检查 `maxInput`，因此这项主线问题不阻断
现有公开功能，但阻断消除这次额外复制。

## 最小复现

在 `release/extlibs/xml.sax/src/sax.xl` 中，把 `__sax_open_memory`
第一个形参从 `point data` 改为 `string data`；在 `reader` 中把
`Buffer data = Buffer()`、`data.append(text)` 和 `data.data()` 调用
改为直接传 `text`，长度使用 `string.byteLen(text)`。同时将
`xml_sax_native.c` 的 `xsax_open_memory` 第一个形参改为
`const char* pData`。随后从 `release` 执行：

```text
./xl.exe run ../dev/extlib_blockers/xml_sax_import_print_repro.xl
```

期望输出 `root`。Windows 最小用例报以下错误；Linux 公开范例
`examples/extlibs/xml_sax.xl` 也在同一位置报错：

```text
extlibs/xml.sax/src/sax.xl:22:12: [sem] undeclared identifier '__xl_std_runtime_take_cstr'
```

撤回上述三处实验改动后，同一用例输出 `root`。在实验改动存在时，
`extlibs/xml.sax/test/boundaries.xl` 仍通过，而公开范例也在同一
符号位置失败，说明问题与编译路径有关，不是 XML 输入内容错误。

## 待判定的修复方向

`__xl_std_runtime_take_cstr` 在标准库内部注册表中提供，包源码直接
引用它。需先查明 `extern string` 变化为何影响该 helper 的可见性：
若是编译器未收集已声明的标准库依赖，应在共享语义/导入层修复；
若包引用私有符号本就没有合同保障，应提供受支持的显式导入或
所有权转换接口，再迁移相关包。完成后复验本包的字符串 ABI、
内嵌 NUL 和 C/XLang 对照；普通扩展库任务不添加私有兼容声明。
