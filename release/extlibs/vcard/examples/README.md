# vCard 双语读取范例

在仓库根目录运行：

```powershell
release\xl.exe run release\extlibs\vcard\examples\folded_contact.xl
release\xl.exe run release\extlibs\vcard\examples\folded_contact_c.c
```

两者用同一个 vCard 4.0 输入，展开姓名、分组邮箱与备注的物理折行，
恢复 TEXT 转义，再用 parse 返回的对象规范写回。固定三行结果见
`folded_contact.expected.txt`。默认不写文件，可在 `--` 后传一个输出
路径保留 `.vcf`；C 使用 `xrtValueRelease` / `xrtFree`，XLang 自动管理。
也可用 `xl build ... -o ...` 编译为程序。

发送端应在完整字符之间折行；读取端也可恢复 UTF-8 字节被折行拆开的
输入，但真正非法的逻辑 UTF-8 仍拒绝。后者由 `test/utf8_folding.c/.xl`
的 C 字节输入夹具验证，不放宽 XLang 源字面量的 UTF-8 要求。

这是常用字段映射，不保留分组名或未知参数，也不是完整 vCard DOM。
本例只证明原生 parse 对象能写回，不替代语言字典 build 的验收；
后者仍有主线 M1，默认门禁保留原创建正例，不能据本例通过签收整包。
原 `vcard.c` 与中央 `release/examples/extlibs/vcard.xl` 展示创建名片的
目标 API，当前语言字典路径仍待主线修复。
