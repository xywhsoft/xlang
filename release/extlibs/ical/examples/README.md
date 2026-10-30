# iCalendar 双语范例

在仓库根目录执行：

```powershell
release\xl.exe run release\extlibs\ical\examples\folded_calendar.xl
release\xl.exe run release\extlibs\ical\examples\folded_calendar_c.c
```

两个程序读取相同的物理折行，恢复中文标题与转义换行，再将解析得到的
日历规范化写回内存。固定三行结果见 `folded_calendar.expected.txt`。
输入不修改，默认不创建文件；可在 `--` 后传一个输出路径保留规范化日历。
C 释放 `xi_parse` 的 xvalue 和 `xi_build` 的
XRT 字符串；XLang 自动管理两者。也可用 `xl build ... -o ...` 生成程序。

发送端应在完整字符之间折行；解析器另外支持恢复在 UTF-8 字节中间
折行的输入，并在展开后拒绝真正非法的 UTF-8。源码字面量必须是合法
UTF-8，因此这种字节级互操作由 `test/utf8_folding.c/.xl` 的 C 输入
夹具验证，范例本身不推荐发送端生成断开的 UTF-8。

本例只展示 `parse` 返回的原生对象再写回，不能替代从 XLang 字典创建
日历的验收。后者仍被主线 M1 阻断，原中央例和完整门禁保留该红项；
不通过 JSON 或私有句柄改写来掩盖。`ical.c` 与中央
`release/examples/extlibs/ical.xl` 是原有创建日历的对照范例。
