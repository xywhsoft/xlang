# 多段包名的局部类型声明

从 `release` 运行：

```powershell
./xl.exe check ../dev/extlib_blockers/ui_auto_dotted_type_declaration.xl
```

期望：已导出的 `ui.auto.Locator` 可用于局部变量类型。
实际：声明行报 `[parse] 语法错误`；同一类型通过
`import ui.auto as uiauto`、`uiauto.Locator` 可编译和运行，见
`release/extlibs/ui.auto/test/named_type.xl`。函数签名中的多段限定类型
另可通过解析，局部变量声明的规则与之不一致。

建议由编译器主线统一类型名解析规则，让多段包限定名可在参数、返回值、
局部变量及容器泛型等类型位置使用；本扩展库任务不修改解析器。
包内教学范例使用正式 `import ... as ...` 语法，不添加旧写法兼容层。
