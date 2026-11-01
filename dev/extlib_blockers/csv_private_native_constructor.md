# CSV 导出类的私有原生构造器仍可被包外调用

状态：blocked-upstream。只记录主线访问控制问题，不在普通扩展库任务中修改编译器。

2026-11-02数据BOM修复后，两平台最终完整门禁仍观测到包外私有构造
负例check退出0；正确的Native普通成员对照仍被拒绝。负构造器没有
执行，不能用新双语例/19原型通过冒充访问隔离已正确。Windows109
阶段98与Linux97阶段91符合要求的剩余项同时含旧M5/M2，Win另有
RFS；当前产品/指纹/完整诊断见
`docs/reports/extlib-csv-bom-values-20261102.json`。下方旧CLI哈希只
描述原登记版本，不用于声明本轮身份；修复责任和建议不变。

## 最小复现

当前 `release/xl.exe` 的 SHA-256：`616888E9DA0097E5449CB4ABD25FC42F87606D836611B3D739860CE57E2D1E3F`。
在 `release` 执行：

```text
xl check extlibs/csv/test/private_constructor.xl
```

`csv.Reader` 是导出类型；它的内存构造器在包源码中声明为
`private extern "xcsv_reader_open_memory" create(point data, uint64 size, ...)`。
负例从包外传入 `null` 与不匹配的 4096 字节长度。实际结果为 `ok`、退出码 0；
预期是访问权限错误。**不要运行该负例**，它只用于编译期访问检查。

对照：`xl check extlibs/csv/test/private_native.xl` 访问私有的
`Writer.fieldNative`，正确报 `no matching callable class member ... access`。
正常 `csv.open` / `csv.create` 包内工厂仍通过检查和 JIT 运行。

## 影响与修复建议

包外可绕过 `csv.open` 的输入指针/长度校验，或绕过 `csv.create` 的方言、
换行和限额检查，直接调用导出类的原生构造器。独立的 C API 仍按 C 合同
显式接收指针和长度；问题在 XLang 类访问控制。

检查语义阶段对 `private extern create` 的标记传播及构造器重载解析；
包内工厂应可访问私有构造器，包外调用应被拒绝。修复后先要求
`private_constructor.xl` 的 `xl check` 非零退出且诊断指向访问控制，
再运行 CSV 的 Windows/Linux 包级 C、XLang、双语范例与完整 AOT 门禁。
本任务不通过取消 `Reader`/`Writer` 类型导出或增设重复包装类改变公开 API。
