# fstr BUG 清单

### BUG-F1: f-string 语义([sem])错误报告的源码位置始终为第 1 行
- 工具：fstr
- 复现：
  ```xlang
  // minimal repro
  func main(...) as int
      string s = f"value={undeclared_var}"
      return 0
  end
  ```
- 现象：编译器报告 `文件:1:25: [sem] f-string 插值表达式不能转换为 string`，
  位置始终是 **第 1 行**（指向文件头注释），而 f-string 实际在第 3 行。
  经多次测试，所有 f-string **语义** 错误均报告 `1:25`，与 f-string 在源码中的真实位置无关。
  对比：f-string 的 **词法/语法** ([parse]) 错误（如空表达式、未闭合大括号）的位置是**正确的**，
  只有 [sem] 阶段的类型检查错误位置错误。
- 期望：[sem] 错误应报告 f-string 所在的实际行号和列号，与 [parse] 错误保持一致。
- 绕过：无；开发时需人工定位 f-string。
- 严重度：中（不影响正确程序，但显著增加调试成本）

### BUG-F2: f-string 插值中引用未声明变量产生误导性错误信息
- 工具：fstr
- 复现：
  ```xlang
  func main(...) as int
      string s = f"value={undeclared_var}"
      return 0
  end
  ```
- 现象：错误信息为 `f-string 插值表达式不能转换为 string`（不能转换为 string），
  而不是预期的 `undeclared identifier 'undeclared_var'`（未声明标识符）。
- 期望：应报告 "未声明标识符" 类型的错误，帮助开发者快速定位问题。
- 绕过：无；需开发者自行排查变量是否已声明。
- 严重度：中

### BUG-F3: 语义检查器未拦截普通表达式中的未声明变量
- 工具：fstr（开发过程中发现）
- 复现：
  ```xlang
  func main(...) as int
      string s = undeclared_var   // 非字符串拼接，直接赋值
      return 0
  end
  ```
- 现象：`xl check` 返回 `ok`（EXIT=0），语义检查通过。
  错误推迟到 C 后端编译阶段才暴露：
  `tcc: __xlvfs__/module.c:233: error: 'undeclared_var' undeclared`。
  错误引用内部 C 文件 `__xlvfs__/module.c` 而非原始 `.xl` 源文件，难以定位。
- 期望：语义检查器应在 `[sem]` 阶段拦截未声明变量，并报告 `.xl` 源文件中的正确位置。
- 绕过：无；依赖 `xl run`/`xl build` 的 C 编译阶段才能发现。
- 严重度：中

### NOTE-F1: f-string 嵌套引号（内层引号与外层不同）已正常工作
- f-string 使用双引号定界时，插值表达式内可使用单引号字符串：`f"{d['key']}"`
- f-string 使用单引号定界时，插值表达式内可使用双引号字符串：`f'{d["key"]}'`
- 两种形式均编译运行正确，结果符合预期。

### NOTE-F2: f-string 插值类型自动转换已正常工作
- int / bool / float / string / var 均可正确插值并自动转为字符串表示。
- float `3.14` 插值结果为 `"3.14"`（非 `"3.140000"`），精度友好。
- bool `true` 插值结果为 `"true"`。
