# abidemo

`abidemo` 同时是 C ABI 声明范例和可执行验证程序。它会调用当前 `xl.exe` 生成 ABI JSON、ABI 文本和 C 头文件，再逐项检查导出信息。

## 运行

在 `demo6` 目录运行：

```bat
release\xl run release\projects\dev\abidemo\abidemo.xl
release\xl build release\projects\dev\abidemo\abidemo.xl -o build\abidemo.exe
build\abidemo.exe
```

在 `release` 目录也可以直接运行：

```bat
xl run projects\dev\abidemo\abidemo.xl
```

任何编译器定位、源文件定位、产物生成或字段断言失败都会返回非零退出码，不会降级为跳过。临时 ABI 文件使用独立随机路径，并在读取后删除。

## 覆盖范围

- `export "c_sym" func` 函数导出。
- 全局变量、常量和字符串导出。
- class 成员函数导出。
- class 多返回值映射为 C `out` 指针。
- `extern` 函数和变量声明。
- ABI JSON 和文本格式。
- `cgen --emit-header` 生成的 C 结构体、符号与类型映射。
- xlang 内部直接调用导出函数、读写导出变量。

预期结果为 61 项检查全部通过，进程退出码为 0。
