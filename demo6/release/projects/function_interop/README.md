# C / xlang 函数互操作范例

该工程同时验证两个方向：

- xlang 的具名、非捕获函数作为普通 C 函数指针传给 C。
- C 函数指针通过生成头文件传给 xlang 导出函数，并在 xlang 中作为带签名 `func` 调用。

运行：

```bat
build.bat
```

`callback_module.h` 完全由 `xl cgen` 生成。C 侧不需要手写 xlang 函数声明，也不需要额外配置 XRT 头文件或链接目录。

构建脚本固定使用同一 `release` 目录中的 `xl.exe`，避免 PATH 中的其他版本污染范例结果。

原始 C 回调没有闭包环境，因此只能接收具名函数或非捕获匿名函数。需要捕获环境时，应使用 XCall，或使用同时接收 `callback + userdata` 的 C API。
